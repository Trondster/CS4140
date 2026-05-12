#include <cstdio>

#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/version.h"

#include "drone_model_ptq_int8.h"

// A global error reporter
tflite::MicroErrorReporter tflite_error_reporter;

// Arena for buffer allocations
const int kTensorArenaSize = 100 * 1024; // Adjust size as needed
uint8_t tensor_arena[kTensorArenaSize];

int main(int argc, char* argv[]) {
  printf("Starting TFLite Micro Inference Example\n");

  // Set up logging (error reporter)
  tflite::ErrorReporter* error_reporter = &tflite_error_reporter;

  // Map the model into a usable data structure
  const tflite::Model* model = tflite::GetModel(tf_model_ptq);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    error_reporter->Report(
        "Model provided is schema version %d not equal to supported version %d.",
        model->version(), TFLITE_SCHEMA_VERSION);
    return 1;
  }

  // Pull in all the operation implementations we need
  tflite::AllOpsResolver resolver;

  // Build an interpreter to run the model with.
  tflite::MicroInterpreter interpreter(
      model, resolver, tensor_arena, kTensorArenaSize, error_reporter);

  // Allocate tensors from the arena
  TfLiteStatus allocate_status = interpreter.AllocateTensors();
  if (allocate_status != kTfLiteOk) {
    error_reporter->Report("AllocateTensors() failed");
    return 1;
  }

  // Obtain pointers to the model's input and output tensors
  TfLiteTensor* current_frame_input = interpreter.input(0);
  TfLiteTensor* diff_frame_input = interpreter.input(1);
  TfLiteTensor* classification_output = interpreter.output(0);
  TfLiteTensor* bbox_output = interpreter.output(1);

  // Check input types (should be INT8 for our PTQ model)
  if (current_frame_input->type != kTfLiteInt8 || diff_frame_input->type != kTfLiteInt8) {
    error_reporter->Report("Expected INT8 input tensors!");
    return 1;
  }

  // Check output types (should be INT8 for our PTQ model)
  if (classification_output->type != kTfLiteInt8 || bbox_output->type != kTfLiteInt8) {
    error_reporter->Report("Expected INT8 output tensors!");
    return 1;
  }

  // --- Prepare dummy input data (replace with actual image data) ---
  // Input shape: (1, InputHeight, InputWidth, 1)
  // Assuming InputHeight=60, InputWidth=80
  // Quantization parameters for inputs are usually 0-255 for images, mapped to int8 range.
  // For this example, let's assume zero_point = 0 and scale = 1/255.0 for simplicity
  // In reality, use the actual quantization parameters from input_details[0]["quantization"]

  // Dummy input values (replace with actual image data)
  // Example: a flat gray image in INT8 format
  int8_t dummy_current_frame_data[60 * 80] = {127}; // All pixels set to 127 (mid-gray)
  int8_t dummy_diff_frame_data[60 * 80] = {127}; // All pixels set to 127 (mid-gray)

  // Fill input tensors
  for (int i = 0; i < current_frame_input->bytes; ++i) {
    current_frame_input->data.int8[i] = dummy_current_frame_data[i % (60*80)]; // Simple fill
    diff_frame_input->data.int8[i] = dummy_diff_frame_data[i % (60*80)]; // Simple fill
  }

  // Run inference
  TfLiteStatus invoke_status = interpreter.Invoke();
  if (invoke_status != kTfLiteOk) {
    error_reporter->Report("Invoke failed!");
    return 1;
  }

  // --- Interpret outputs ---
  // Classification Output: (1,) single float (sigmoid output)
  // Bounding Box Output: (4,) floats (x_center, y_center, width, height)

  // Dequantize classification output
  // The output tensor has quantization parameters: scale and zero_point
  // Real_value = (int8_value - zero_point) * scale
  float classification_scale = classification_output->params.scale;
  int32_t classification_zero_point = classification_output->params.zero_point;
  float predicted_classification = (classification_output->data.int8[0] - classification_zero_point) * classification_scale;

  // Dequantize bounding box output
  float bbox_scale = bbox_output->params.scale;
  int32_t bbox_zero_point = bbox_output->params.zero_point;
  float predicted_bbox[4];
  for (int i = 0; i < 4; ++i) {
    predicted_bbox[i] = (bbox_output->data.int8[i] - bbox_zero_point) * bbox_scale;
  }

  // --- Convert normalized bounding box to pixel coordinates for a 160x120 screen ---
  const int screen_width = 160;
  const int screen_height = 120;

  // Normalized values from predicted_bbox:
  // predicted_bbox[0]: x_center
  // predicted_bbox[1]: y_center
  // predicted_bbox[2]: width
  // predicted_bbox[3]: height

  float x_center_norm = predicted_bbox[0];
  float y_center_norm = predicted_bbox[1];
  float width_norm = predicted_bbox[2];
  float height_norm = predicted_bbox[3];

  // Calculate pixel width and height
  int pixel_width = (int)(width_norm * screen_width);
  int pixel_height = (int)(height_norm * screen_height);

  // Calculate top-left x and y coordinates
  int top_left_x = (int)((x_center_norm - width_norm / 2.0) * screen_width);
  int top_left_y = (int)((y_center_norm - height_norm / 2.0) * screen_height);

  printf("\nInference Results:\n");
  printf("  Predicted Classification (prob): %.4f (Drone if > 0.5)\n", predicted_classification);
  printf("  Predicted Bounding Box (normalized): [x_c=%.4f, y_c=%.4f, w=%.4f, h=%.4f]\n",
         predicted_bbox[0], predicted_bbox[1], predicted_bbox[2], predicted_bbox[3]);
  printf("  Predicted Bounding Box (pixels on 160x120 screen): [x_tl=%d, y_tl=%d, w=%d, h=%d]\n",
         top_left_x, top_left_y, pixel_width, pixel_height);

  printf("Inference successful!\n");

  return 0;
}
