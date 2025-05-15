def main():
    # your training code here, for example:
    from ultralytics import YOLO
    model = YOLO('yolov8m.pt')  # or your custom model
    model.train(data='D:\project\det.v8i.yolov8_split\data.yaml', epochs=50)

if __name__ == "__main__":
    from multiprocessing import freeze_support
    freeze_support()  # optional for pyinstaller use
    main()