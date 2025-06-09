import cv2
import easyocr
from ultralytics import YOLO
import time
import serial
import queue
from datetime import datetime, timedelta
import logging
import os

# Cấu hình logging
logging.basicConfig(filename='license_plate.log', level=logging.INFO, 
                    format='%(asctime)s - %(message)s')

# Hàng đợi lưu trữ biển số từ camera vào
license_plate_queue = queue.Queue()

def count_matching_chars(plate1, plate2):
    """
    Đếm số ký tự trùng khớp giữa hai chuỗi, bỏ qua khoảng trắng và không phân biệt hoa thường.
    """
    plate1 = plate1.lower().replace(" ", "").replace(".", "")
    plate2 = plate2.lower().replace(" ", "").replace(".", "")
    set1 = set(plate1)
    set2 = set(plate2)
    matching_chars = len(set1.intersection(set2))
    return matching_chars  

def detect_license_plate(camera_id, output_file, ser=None, is_entry=True, model=None, reader=None):
    """     
    Hàm phát hiện và nhận diện biển số, lưu vào file/queue, gửi tín hiệu qua Serial.
    """
    if not model or not reader:
        logging.error("Mô hình YOLO hoặc EasyOCR không được cung cấp")
        return None

    cap = cv2.VideoCapture(camera_id, cv2.CAP_DSHOW)
    if not cap.isOpened():
        logging.error(f"Không thể mở camera {camera_id}")
        print(f"Không thể mở camera {camera_id}")
        return None

    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

    last_plate_text = ""
    last_detection_time = 0
    detection_delay = 0.3
    plate_detected = False
    timeout = 30
    start_time = time.time()
    process_interval = 0.1
    last_processed_time = 0

    try:
        while time.time() - start_time < timeout:
            if time.time() - last_processed_time < process_interval:
                continue
            last_processed_time = time.time()

            ret, frame = cap.read()
            if not ret:
                logging.error(f"Không thể đọc frame từ camera {camera_id}")
                print(f"Không thể đọc frame từ camera {camera_id}")
                break

            results = model(frame, device="cuda")
            if not results:
                print(f"Camera {camera_id}: Không có kết quả từ YOLO")
                continue

            for r in results:
                for box in r.boxes:
                    x1, y1, x2, y2 = map(int, box.xyxy[0])
                    conf = box.conf[0]
                    print(f"Camera {camera_id}: Phát hiện đối tượng với độ tin cậy: {conf}")
                    if conf > 0.4:
                        cv2.rectangle(frame, (x1, y1), (x2, y2), (0, 255, 0), 2)
                        plate = frame[y1:y2, x1:x2]
                        plate = cv2.resize(plate, (200, 50))
                        plate_gray = cv2.cvtColor(plate, cv2.COLOR_BGR2GRAY)
                        plate_gray = cv2.equalizeHist(plate_gray)
                        text = reader.readtext(plate_gray, detail=0)
                        print(f"Camera {camera_id}: Kết quả từ EasyOCR: {text}")
                        if not text:
                            cv2.putText(frame, "Không đọc được biển số", (x1, y1 - 10), 
                                        cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 255), 2)
                            continue
                        plate_text = ' '.join(text).strip()
                        print(f"Camera {camera_id}: Văn bản biển số: {plate_text}")
                        if plate_text and plate_text != last_plate_text:
                            last_plate_text = plate_text
                            last_detection_time = time.time()
                            plate_detected = True
                            print(f"Camera {camera_id}: Đã phát hiện biển số mới: {plate_text}")
                        if plate_text:
                            cv2.putText(frame, plate_text, (x1, y1 - 10), 
                                        cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)

            if plate_detected and last_plate_text and (time.time() - last_detection_time) >= detection_delay:
                logging.info(f"Camera {camera_id} nhận diện biển số: {last_plate_text}")
                print(f"Camera {camera_id} nhận diện biển số: {last_plate_text}")
                
                if is_entry:
                    license_plate_queue.put((last_plate_text, datetime.now()))
                    output_file.write(f"{last_plate_text}\n")
                    output_file.flush()
                    if ser and ser.is_open:
                        ser.write("RUN_SERVO1\n".encode('utf-8'))
                        logging.info("Đã gửi tín hiệu RUN_SERVO1 đến ESP32")
                        print("Đã gửi tín hiệu RUN_SERVO1 đến ESP32")
                        start_time = time.time()
                        while time.time() - start_time < 5:
                            if ser.in_waiting > 0:
                                response = ser.readline().decode('utf-8').strip()
                                if response == "SERVO1_DONE":
                                    logging.info("Nhận được phản hồi từ ESP32: SERVO1_DONE")
                                    print("Nhận được phản hồi từ ESP32: SERVO1_DONE")
                                    break
                else:
                    plate_matched = False
                    temp_queue = queue.Queue()
                    while not license_plate_queue.empty():
                        plate, timestamp = license_plate_queue.get()
                        if datetime.now() - timestamp > timedelta(hours=1):
                            continue
                        matching_chars = count_matching_chars(plate, last_plate_text)
                        if matching_chars >= 4:
                            logging.info(f"Biển số {last_plate_text} có {matching_chars} ký tự khớp với {plate}, mở barrier ra")
                            print(f"Biển số {last_plate_text} có {matching_chars} ký tự khớp với {plate}, mở barrier ra")
                            plate_matched = True
                        else:
                            temp_queue.put((plate, timestamp))
                    while not temp_queue.empty():
                        license_plate_queue.put(temp_queue.get())
                    
                    if plate_matched and ser and ser.is_open:
                        ser.write("RUN_SERVO2\n".encode('utf-8'))
                        logging.info("Đã gửi tín hiệu RUN_SERVO2 đến ESP32")
                        print("Đã gửi tín hiệu RUN_SERVO2 đến ESP32")
                        start_time = time.time()
                        while time.time() - start_time < 5:
                            if ser.in_waiting > 0:
                                response = ser.readline().decode('utf-8').strip()
                                if response == "SERVO2_DONE":
                                    logging.info("Nhận được phản hồi từ ESP32: SERVO2_DONE")
                                    print("Nhận được phản hồi từ ESP32: SERVO2_DONE")
                                    break
                    elif not plate_matched:
                        logging.warning(f"Biển số {last_plate_text} không có đủ ký tự khớp")
                        print(f"Biển số {last_plate_text} không có đủ ký tự khớp")
                
                break

            cv2.imshow(f"License Plate Detection - Camera {camera_id}", frame)
            if cv2.waitKey(1) & 0xFF == ord('q'):
                print(f"Camera {camera_id}: Người dùng nhấn 'q', thoát vòng lặp")
                break
        else:
            print(f"Camera {camera_id}: Hết thời gian chờ (timeout), không phát hiện được biển số")
    finally:
        cap.release()
        cv2.destroyAllWindows()

    return last_plate_text

def main():
    output_file_path = "license_plates.txt"
    serial_port = "COM7"
    baud_rate = 115200
    model_path = r"C:\Users\ADMIN\Do_an1\runs\detect\train\weights\best.pt"

    # Kiểm tra mô hình YOLO
    if not os.path.exists(model_path):
        logging.error("Không tìm thấy file mô hình YOLO")
        print("Không tìm thấy file mô hình YOLO")
        return

    # Tải mô hình YOLO và EasyOCR
    model = YOLO(model_path)
    reader = easyocr.Reader(['en', 'vi'], gpu=True)

    # Kết nối Serial
    try:    
        ser = serial.Serial(serial_port, baud_rate, timeout=1)
        time.sleep(2)
    except serial.SerialException as e:
        logging.error(f"Lỗi kết nối Serial: {e}")
        print(f"Lỗi kết nối Serial: {e}")
        return

    # Mở file để ghi biển số
    with open(output_file_path, 'a', encoding='utf-8') as output_file:
        logging.info("Hệ thống khởi động, chờ tín hiệu từ ESP32...")
        print("Chờ tín hiệu từ ESP32...")
        
        # Danh sách các lệnh hợp lệ
        valid_commands = {"CAR_ENTRY", "CAR_EXIT", "SERVO1_DONE", "SERVO2_DONE"}
        
        while True:
            try:
                if ser.in_waiting > 0:
                    command = ser.readline().decode('utf-8').strip()
                    # Chỉ in và xử lý nếu là lệnh hợp lệ
                    if command in valid_commands:
                        print(f"Nhận tín hiệu từ ESP32: {command}")
                        if command == "CAR_ENTRY":
                            logging.info("Phát hiện xe ở lối vào, kích hoạt camera 0...")
                            print("Phát hiện xe ở lối vào, kích hoạt camera 0...")
                            detect_license_plate(1, output_file, ser, True, model, reader)
                        elif command == "CAR_EXIT":
                            logging.info("Phát hiện xe ở lối ra, kích hoạt camera 1...")
                            print("Phát hiện xe ở lối ra, kích hoạt camera 1...")
                            detect_license_plate(0, output_file, ser, False, model, reader)
                        elif command in ["SERVO1_DONE", "SERVO2_DONE"]:
                            print(f"Bỏ qua tín hiệu: {command}")
                            continue
                time.sleep(0.1)  # Giảm tải CPU
            except serial.SerialException as e:
                logging.error(f"Lỗi giao tiếp Serial: {e}")
                print(f"Lỗi giao tiếp Serial: {e}")
                break
            except UnicodeDecodeError:
                logging.error("Lỗi giải mã dữ liệu Serial")
                print("Lỗi giải mã dữ liệu Serial")
                continue

    ser.close()

if __name__ == "__main__":
    main()