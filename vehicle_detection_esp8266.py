import cv2
import time
import requests
import logging
from threading import Thread
from ultralytics import YOLO
from urllib3.util.retry import Retry
from requests.adapters import HTTPAdapter
from twilio.rest import Client

# Twilio setup (REPLACE with your real Twilio creds!)
account_sid = ''
auth_token = ''
twilio_number = ''
receiver_number = ''
client = Client(account_sid, auth_token)

# ESP8266 config
ESP_IP = ""
MAX_RETRIES = 3
RETRY_BACKOFF = 0.5
REQUEST_TIMEOUT = 5

# Logging setup
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

# Setup requests session with retries
session = requests.Session()
retries = Retry(total=MAX_RETRIES, backoff_factor=RETRY_BACKOFF,
                status_forcelist=[408, 429, 500, 502, 503, 504])
session.mount('http://', HTTPAdapter(max_retries=retries))

# 🔁 Threaded Video Capture Class
class VideoStream:
    def __init__(self, src=0):
        self.cap = cv2.VideoCapture(src)
        if not self.cap.isOpened():
            logger.error("Failed to open video source")
            raise RuntimeError("Video source not available")
        self.ret, self.frame = self.cap.read()
        self.running = True
        Thread(target=self.update, daemon=True).start()

    def update(self):
        while self.running:
            self.ret, self.frame = self.cap.read()

    def read(self):
        return self.ret, self.frame

    def stop(self):
        self.running = False
        self.cap.release()

# 🚀 ESP + SMS
def send_to_esp(vehicle_count, ambulance_detected):
    ambulance_value = 1 if ambulance_detected else 0
    url = f"http://{ESP_IP}/update?vehicles={vehicle_count}&ambulance={ambulance_value}"
    try:
        logger.info(f"Sending data: vehicles={vehicle_count}, ambulance={ambulance_value}")
        response = session.get(url, timeout=REQUEST_TIMEOUT)
        response.raise_for_status()
        logger.info(f"ESP8266 response: {response.text}")
        return True
    except Exception as e:
        logger.error(f"ESP8266 send error: {e}")
        return False

def send_sms_alert(message):
    try:
        msg = client.messages.create(body=message, from_=twilio_number, to=receiver_number)
        logger.info(f"SMS sent: {msg.sid}")
    except Exception as e:
        logger.error(f"Failed to send SMS: {e}")

# 🧠 Main YOLO Loop
def main():
    try:
        model_path = r"D:\project\Final test\yolo\runs\detect\train3\weights\last.pt"
        model = YOLO(model_path)
    except Exception as e:
        logger.error(f"Failed to load YOLO model: {e}")
        return

    video_source = 0  # DroidCam virtual cam index (0 or 1 typically)
    try:
        stream = VideoStream(video_source)
    except RuntimeError:
        return

    logger.info("Started threaded vehicle + ambulance detection")
    last_send_time = 0
    send_interval = 2
    last_sms_time = 0
    sms_cooldown = 30

    while True:
        ret, frame = stream.read()
        if not ret:
            logger.warning("Frame drop detected")
            time.sleep(0.3)
            continue

        try:
            results = model(frame, conf=0.5)
            vehicle_count = 0
            ambulance_detected = False

            for box in results[0].boxes:
                cls_id = int(box.cls.item())
                label = model.names[cls_id].lower()

                if label in ['car', 'truck']:
                    vehicle_count += 1
                elif label == 'ambulance':
                    vehicle_count += 1
                    ambulance_detected = True

                if label in ['car', 'ambulance']:
                    x1, y1, x2, y2 = map(int, box.xyxy[0])
                    color = (0, 255, 0) if label == 'car' else (0, 0, 255)
                    cv2.rectangle(frame, (x1, y1), (x2, y2), color, 2)
                    cv2.putText(frame, label.capitalize(), (x1, y1 - 10),
                                cv2.FONT_HERSHEY_SIMPLEX, 0.6, color, 2)

            cv2.putText(frame, f"Vehicles: {vehicle_count}", (10, 50),
                        cv2.FONT_HERSHEY_SIMPLEX, 1, (255, 0, 0), 2)
            cv2.putText(frame, f"Ambulance: {'YES' if ambulance_detected else 'NO'}", (10, 90),
                        cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 0, 255), 2)

            current_time = time.time()
            if current_time - last_send_time >= send_interval:
                if send_to_esp(vehicle_count, ambulance_detected):
                    last_send_time = current_time
                    cv2.putText(frame, "✅ Data Sent", (10, 130),
                                cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)

            if current_time - last_sms_time >= sms_cooldown:
                if ambulance_detected:
                    send_sms_alert("🚨 Ambulance detected behind you! Please give way.")
                    last_sms_time = current_time
                elif vehicle_count > 5:
                    send_sms_alert("🚦 Traffic is moderate, please drive carefully.")
                    last_sms_time = current_time

            cv2.imshow("Vehicle Detection", frame)

        except Exception as e:
            logger.error(f"Error processing frame: {e}")

        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    stream.stop()
    cv2.destroyAllWindows()
    logger.info("Detection stopped")

if __name__ == "__main__":
    main()
