"""
NoMoreDay Interactive Image Cropper
Purpose: A GUI tool to manually crop and standardize raw AI-generated images (1024x1024)
         to the game's standard 128x128 format with proper padding.
Usage: python scripts/cut_picture.py <directory_path>
       Controls: Mouse drag to select, F5/C to save and move to next.
"""
import cv2
import numpy as np
import os
import sys
import argparse

class ImageCropper:
    def __init__(self, directory):
        self.directory = directory
        # 获取目录下所有png文件
        self.files = [f for f in os.listdir(directory) if f.lower().endswith('.png')]
        self.files.sort() # 排序，保证顺序
        self.current_index = 0
        
        # 鼠标交互状态
        self.drawing = False # 是否正在拖拽
        self.mode_crop = False # 是否按下了F5进入截图模式
        self.ix, self.iy = -1, -1 # 起始坐标
        self.ex, self.ey = -1, -1 # 结束坐标
        self.img_raw = None # 原始图像
        self.img_display = None # 用于显示的图像（包含绘制的框）

    def resize_and_pad(self, crop_img):
        """
        将截取的图像转换为128x128，长边缩放，短边padding透明像素
        """
        h, w = crop_img.shape[:2]
        target_size = 128

        # 1. 计算缩放比例
        scale = target_size / max(h, w)
        new_w = int(w * scale)
        new_h = int(h * scale)

        # 2. 缩放图像 (使用线性插值，适合放大和缩小)
        resized = cv2.resize(crop_img, (new_w, new_h), interpolation=cv2.INTER_LINEAR)

        # 3. 创建 128x128 的透明背景 (BGRA)
        # 格式: [高度, 宽度, 通道数], 数据类型 uint8
        final_img = np.zeros((target_size, target_size, 4), dtype=np.uint8)

        # 4. 计算中心位置并粘贴
        x_offset = (target_size - new_w) // 2
        y_offset = (target_size - new_h) // 2
        
        # 将缩放后的图像放入中心
        final_img[y_offset:y_offset+new_h, x_offset:x_offset+new_w] = resized

        return final_img

    def mouse_callback(self, event, x, y, flags, param):
        # 只有在截图模式下才响应鼠标
        if not self.mode_crop:
            return

        # 限制坐标在图像范围内 (0 ~ 1023)
        h, w = self.img_raw.shape[:2]
        x = max(0, min(x, w - 1))
        y = max(0, min(y, h - 1))

        if event == cv2.EVENT_LBUTTONDOWN:
            self.drawing = True
            self.ix, self.iy = x, y
            self.ex, self.ey = x, y

        elif event == cv2.EVENT_MOUSEMOVE:
            if self.drawing:
                self.ex, self.ey = x, y
                # 实时绘制矩形框
                self.img_display = self.img_raw.copy()
                cv2.rectangle(self.img_display, (self.ix, self.iy), (self.ex, self.ey), (0, 255, 0), 2)
                cv2.imshow('Image Processor', self.img_display)

        elif event == cv2.EVENT_LBUTTONUP:
            self.drawing = False
            self.ex, self.ey = x, y
            # 绘制最终框
            self.img_display = self.img_raw.copy()
            cv2.rectangle(self.img_display, (self.ix, self.iy), (self.ex, self.ey), (0, 255, 0), 2)
            cv2.imshow('Image Processor', self.img_display)
            
            # 自动触发处理逻辑
            self.process_selection()

    def process_selection(self):
        # 确定左上角和右下角
        x1, x2 = sorted([self.ix, self.ex])
        y1, y2 = sorted([self.iy, self.ey])
        
        w = x2 - x1
        h = y2 - y1

        if w <= 0 or h <= 0:
            print("选区无效，请重新选择。")
            return

        # 截取 ROI (Region of Interest)
        roi = self.img_raw[y1:y2, x1:x2]

        # 处理图像 (缩放 + Padding)
        result_img = self.resize_and_pad(roi)

        # 覆盖保存
        current_file = os.path.join(self.directory, self.files[self.current_index])
        cv2.imwrite(current_file, result_img)
        print(f"已处理并覆盖: {self.files[self.current_index]}")

        # 移动到下一张
        self.next_image()

    def next_image(self):
        self.current_index += 1
        self.mode_crop = False # 重置模式
        self.load_current_image()

    def load_current_image(self):
        if self.current_index >= len(self.files):
            print("所有图像处理完成！")
            cv2.destroyAllWindows()
            sys.exit(0)

        file_path = os.path.join(self.directory, self.files[self.current_index])
        # 读取图像，保留 Alpha 通道 (IMREAD_UNCHANGED)
        self.img_raw = cv2.imread(file_path, cv2.IMREAD_UNCHANGED)

        if self.img_raw is None:
            print(f"无法读取 {file_path}，跳过...")
            self.next_image()
            return

        # 如果图像没有 Alpha 通道 (比如是 JPG 转的 PNG)，强制转为 BGRA
        if len(self.img_raw.shape) == 2: # 灰度
            self.img_raw = cv2.cvtColor(self.img_raw, cv2.COLOR_GRAY2BGRA)
        elif self.img_raw.shape[2] == 3: # BGR
            self.img_raw = cv2.cvtColor(self.img_raw, cv2.COLOR_BGR2BGRA)

        self.img_display = self.img_raw.copy()
        cv2.imshow('Image Processor', self.img_display)
        print(f"正在处理 ({self.current_index + 1}/{len(self.files)}): {self.files[self.current_index]}")
        print("按 'F5' 开始框选截图，按 'Esc' 退出。")

    def run(self):
        if not self.files:
            print("指定目录下没有找到 PNG 文件。")
            return

        cv2.namedWindow('Image Processor')
        cv2.setMouseCallback('Image Processor', self.mouse_callback)
        
        self.load_current_image()

        while True:
            # 等待按键
            k = cv2.waitKey(10) & 0xFF

            if k == 27: # Esc 退出
                break
            
            # F5 键处理
            # 注意：不同系统 F5 的键值可能不同。
            # 通常 Windows/Linux 上 OpenCV 的功能键映射比较复杂。
            # 这里同时监听 F5 的常见键值，并建议如果 F5 无效可以使用 'c' 键作为备用
            if k == 194 or k == 0 or k == ord('c'): 
                # 注: 194 是某些环境下 F5 的值，但 OpenCV waitKey 对功能键支持不稳定
                # 建议在控制台提示用户如果 F5 没反应，可以使用 'c' 键
                if not self.mode_crop:
                    self.mode_crop = True
                    print(">>> 进入截图模式：请在窗口中框选区域 (松开鼠标自动保存)")
                    # 视觉提示：可以在图像上画个提示或者改变窗口标题（OpenCV不支持改标题，只能print）

if __name__ == "__main__":
    # 解析命令行参数
    if len(sys.argv) < 2:
        print("使用方法: python script.py <图片目录路径>")
        sys.exit(1)
    
    target_dir = sys.argv[1]
    if not os.path.isdir(target_dir):
        print("错误: 目录不存在")
        sys.exit(1)

    cropper = ImageCropper(target_dir)
    cropper.run()
