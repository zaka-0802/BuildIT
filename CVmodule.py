# Few-Shot Object Recognition with YOLOv8 and MobileNetV2
# This notebook implements a system that can recognize objects with only a few labeled examples per class

import os
import cv2
import numpy as np
import torch
import matplotlib.pyplot as plt
from PIL import Image
import torchvision.transforms as transforms
from torchvision.models import mobilenet_v2
from sklearn.metrics.pairwise import cosine_similarity
from google.colab import files
import zipfile
import random
from tqdm import tqdm
from ultralytics import YOLO
import os
from PIL import Image

# Set random seed for reproducibility
np.random.seed(42)
torch.manual_seed(42)
random.seed(42)

class FewShotObjectRecognition:
    def __init__(self):
        # Initialize YOLOv8 for object detection only
        self.detector = YOLO('yolov8s.pt')

        # Initialize MobileNetV2 for feature extraction
        self.feature_extractor = mobilenet_v2(pretrained=True)
        # Remove the classification layer
        self.feature_extractor = torch.nn.Sequential(*list(self.feature_extractor.children())[:-1])
        self.feature_extractor.eval()

        # Move to GPU if available
        self.device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
        self.feature_extractor.to(self.device)

        # Image preprocessing
        self.transform = transforms.Compose([
            transforms.Resize((224, 224)),
            transforms.ToTensor(),
            transforms.Normalize(mean=[0.485, 0.456, 0.406], std=[0.229, 0.224, 0.225])
        ])

        # Data augmentation for support set
        self.augmentation = transforms.Compose([
            transforms.RandomHorizontalFlip(),
            transforms.RandomRotation(15),
            transforms.ColorJitter(brightness=0.1, contrast=0.1, saturation=0.1, hue=0.1),
            transforms.Resize((224, 224)),
            transforms.ToTensor(),
            transforms.Normalize(mean=[0.485, 0.456, 0.406], std=[0.229, 0.224, 0.225])
        ])

        # Store class embeddings
        self.class_embeddings = {}
        self.class_names = []

        # Confidence threshold for classification
        self.confidence_threshold = 0.40

        print(f"Initialized model on {self.device}")

    def detect_objects(self, image_path):
        """Detect objects using YOLOv8 and return cropped images"""
        # Run YOLOv8 detection (disable printing)
        results = self.detector(image_path, verbose=False)

        # Get the original image
        if isinstance(image_path, str):
            original_img = cv2.imread(image_path)
            original_img = cv2.cvtColor(original_img, cv2.COLOR_BGR2RGB)
        else:
            original_img = image_path

        crops = []
        boxes = []

        # Extract bounding boxes and crop objects
        for result in results:
            for box in result.boxes.xyxy:
                x1, y1, x2, y2 = box.cpu().numpy().astype(int)
                # Ensure the box is valid
                if x1 < x2 and y1 < y2 and x1 >= 0 and y1 >= 0 and x2 < original_img.shape[1] and y2 < original_img.shape[0]:
                    crop = original_img[y1:y2, x1:x2]
                    if crop.size > 0:  # Ensure crop is not empty
                        crops.append(crop)
                        boxes.append([x1, y1, x2, y2])

        return crops, boxes, original_img

    def extract_features(self, image, augment=False):
        """Extract features from an image using MobileNetV2"""
        # Convert to PIL Image if it's a numpy array
        if isinstance(image, np.ndarray):
            image = Image.fromarray(image)

        # Apply transformations
        if augment:
            # Apply multiple augmentations and average the embeddings
            embeddings = []
            for _ in range(5):  # 5 different augmentations
                img_tensor = self.augmentation(image).unsqueeze(0).to(self.device)
                with torch.no_grad():
                    embedding = self.feature_extractor(img_tensor)
                    # Flatten the embedding
                    embedding = embedding.view(embedding.size(0), -1)
                    embedding = embedding.squeeze().cpu().numpy()
                    # Normalize the embedding
                    embedding = embedding / np.linalg.norm(embedding)
                    embeddings.append(embedding)

            # Average the embeddings
            embedding = np.mean(embeddings, axis=0)
            # Normalize again after averaging
            embedding = embedding / np.linalg.norm(embedding)
        else:
            img_tensor = self.transform(image).unsqueeze(0).to(self.device)
            with torch.no_grad():
                embedding = self.feature_extractor(img_tensor)
                # Flatten the embedding
                embedding = embedding.view(embedding.size(0), -1)
                embedding = embedding.squeeze().cpu().numpy()
                # Normalize the embedding
                embedding = embedding / np.linalg.norm(embedding)

        return embedding

    def add_class(self, class_name, support_images):
        """Add a new class with support images"""
        print(f"Adding class: {class_name} with {len(support_images)} support images")

        # Extract features from support images with augmentation
        embeddings = []
        for img in support_images:
            embedding = self.extract_features(img, augment=True)
            embeddings.append(embedding)

        # Average the embeddings to get a class prototype
        class_embedding = np.mean(embeddings, axis=0)
        # Normalize the class embedding
        class_embedding = class_embedding / np.linalg.norm(class_embedding)

        # Store the class embedding
        self.class_embeddings[class_name] = class_embedding
        if class_name not in self.class_names:
            self.class_names.append(class_name)

        print(f"Added class {class_name}. Total classes: {len(self.class_names)}")

    def recognize_objects(self, image_path, top_k=3):
        """Detect and recognize objects in an image"""
        # Detect objects
        crops, boxes, original_img = self.detect_objects(image_path)

        if not crops:
            print("No objects detected!")
            return original_img, []

        results = []

        # Recognize each detected object
        for i, crop in enumerate(crops):
            # Extract features
            embedding = self.extract_features(crop)

            # Calculate similarity with each class
            similarities = {}
            for class_name in self.class_names:
                class_embedding = self.class_embeddings[class_name]
                # Ensure embeddings are 1D arrays for cosine_similarity
                similarity = cosine_similarity(embedding.reshape(1, -1), class_embedding.reshape(1, -1))[0][0]
                similarities[class_name] = similarity

            # Sort by similarity
            sorted_similarities = sorted(similarities.items(), key=lambda x: x[1], reverse=True)

            # Get top-k predictions
            top_predictions = sorted_similarities[:min(top_k, len(sorted_similarities))]

            # Check if the highest similarity is above the threshold
            if top_predictions and top_predictions[0][1] >= self.confidence_threshold:
                predicted_class = top_predictions[0][0]
            else:
                predicted_class = "Unknown"

            # Store results
            results.append({
                'box': boxes[i],
                'predicted_class': predicted_class,
                'confidence': top_predictions[0][1] if top_predictions else 0,
                'top_predictions': top_predictions
            })

        return original_img, results

    def visualize_results(self, image, results):
        """Visualize detection and recognition results"""
        # Create a copy of the image
        vis_image = image.copy()

        # Draw bounding boxes and labels
        for result in results:
            x1, y1, x2, y2 = result['box']
            predicted_class = result['predicted_class']
            confidence = result['confidence']

            # Draw bounding box
            cv2.rectangle(vis_image, (x1, y1), (x2, y2), (100, 255, 100), 2)

            # Draw label
            label = f"{predicted_class}: {confidence:.2f}"
            cv2.putText(vis_image, label, (x1, y1-10), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (100, 255, 100), 2)

        # Display the image
        plt.figure(figsize=(12, 8))
        plt.imshow(vis_image)
        plt.axis('off')
        plt.show()

        # Print detailed results
        for i, result in enumerate(results):
            print(f"Object {i+1}:")
            print(f"  Predicted class: {result['predicted_class']}")
            print(f"  Confidence: {result['confidence']:.4f}")
            print("  Top predictions:")
            for cls, conf in result['top_predictions']:
                print(f"    {cls}: {conf:.4f}")
            print()

    def save_model(self, filename="few_shot_model.npz"):
        """Save the model (class embeddings and names)"""
        # Convert embeddings to a dictionary of numpy arrays
        embeddings_dict = {name: self.class_embeddings[name] for name in self.class_names}

        # Save to file
        np.savez(filename,
                 class_names=np.array(self.class_names),
                 **embeddings_dict)

        print(f"Model saved to {filename}")

    def load_model(self, filename="few_shot_model.npz"):
        """Load the model (class embeddings and names)"""
        # Load from file
        data = np.load(filename, allow_pickle=True)

        # Extract class names
        self.class_names = data['class_names'].tolist()

        # Extract embeddings
        self.class_embeddings = {}
        for name in self.class_names:
            self.class_embeddings[name] = data[name]

        print(f"Model loaded from {filename} with {len(self.class_names)} classes")

# Main execution
def main():
    # Initialize the model
    model = FewShotObjectRecognition()

    while True:
        print("\n=== Few-Shot Object Recognition System ===")
        print("1. Add a new class")
        print("2. Test the model")
        print("3. Save the model")
        print("4. Load a model")
        print("5. Exit")

        choice = input("Enter your choice (1-5): ")

        if choice == '1':
            # Add a new class
            class_name = input("Enter class name: ")
            print("Upload 5 images for this class...")

            uploaded = files.upload()
            support_images = []

            for filename in uploaded.keys():
                try:
                    img = Image.open(filename)
                    img = np.array(img)
                    if len(img.shape) == 2:  # Convert grayscale to RGB
                        img = cv2.cvtColor(img, cv2.COLOR_GRAY2RGB)
                    elif img.shape[2] == 4:  # Convert RGBA to RGB
                        img = img[:, :, :3]
                    support_images.append(img)
                except Exception as e:
                    print(f"Error loading {filename}: {e}")

            if len(support_images) > 0:
                model.add_class(class_name, support_images)
            else:
                print("No valid images uploaded!")

        elif choice == '2':
            # Test the model


# Define your folder path (e.g., from Google Drive or local Colab environment)
            folder_path = '/content/Tang'  # change this to your actual path
            for filename in os.listdir(folder_path):
             if filename.lower().endswith(('.png', '.jpg', '.jpeg', '.bmp', '.gif')):  # image formats
                file_path = os.path.join(folder_path, filename)
                try:
                    image, results = model.recognize_objects(file_path)
                    model.visualize_results(image, results)
                except Exception as e:
                    print(f"Error processing {filename}: {e}")

        elif choice == '3':
            # Save the model
            filename = input("Enter filename to save (default: few_shot_model.npz): ")
            if not filename:
                filename = "few_shot_model.npz"
            model.save_model(filename)
            files.download(filename)

        elif choice == '4':
            # Load a model
            print("Upload a model file (.npz)...")
            uploaded = files.upload()

            for filename in uploaded.keys():
                try:
                    model.load_model(filename)
                except Exception as e:
                    print(f"Error loading model from {filename}: {e}")
                break

        elif choice == '5':
            # Exit
            print("Exiting...")
            break

        else:
            print("Invalid choice! Please enter a number between 1 and 5.")

if __name__ == "__main__":
    main()