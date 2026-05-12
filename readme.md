# USN CS4140 project spring 2026

***
This is the repo for the CS4140 project spring 2026 by candidates 8502 and 8515.

Set up the board with camera and screen (but no microphone!) as detailed in the [main CS4140 repo](https://github.com/aiunderstand/USN-CS4140-EmbeddedAI/blob/main/examples/ov7670-fifo-camera/README.md).

The various apps use `button0`, `button1`, and `button2`. 

Applications for the board - build and flash these onto the board.
* **camera-capture_cpp** - captures images. Run the Python script at `lib\python\receive_image.py` to capture images, for example `python .\receive_image.py COM4`.
* **camera-preprocessing_cpp** - displays various preprocessing filters. 
* **camera-show-on-screen** - simple app displaying captured images on the screen.
* **drone_inference_base** - runs inference on the camera input, showing a bounding box on the screen if a drone is inferred.

Other folders:
* **image-labeler** - run and build to organize images. Uses C#.
* **collision-cleanup** and **collision-sameset** - throwaway applications to clean up image data taken by earlier iterations of camera-capture_cpp.
* **lib** - contains libraries for the board applications.
* **train** - contains the Jupyter notebook for model training. Open `lib\drone-classification.ipynb` to run the model. To run it, upload the dataset.zip file found at [https://drive.google.com/file/d/1E1GRKtAh0No5De3GQojph6V3vIS9nVSM/view?usp=drive_link](https://drive.google.com/file/d/1E1GRKtAh0No5De3GQojph6V3vIS9nVSM/view?usp=drive_link) as `dataset.zip`.
* **documentation** - contains some documentation files, as well as Jupyter notebooks containing some test models.

## Extended software documentation

### camera-show-on-screen
The screen should show the camera input.

### camera-preprocessing_cpp
Press `button0` to freeze the frame, and press `button1` to cycle through the modes. The modes are:
* Direct passthrough - shows the camera color input.
* Grayscale - shows the camera input converted to grayscale.
* Downscaled grayscale 2x - shows the camera input converted to grayscale downsampled 2x.
* Downscaled grayscale 3x - shows the camera input converted to grayscale downsampled 3x.
* Downscaled grayscale 4x - shows the camera input converted to grayscale downsampled 4x.
* Left Sobel - shows a grayscale Left Sobel transformation, enhancing outlines.
* Outline Sobel - shows a grayscale Outline Sobel transformation, enhancing outlines.
* Diff grayscale Abs - experimental diff image algorithm
* Diff grayscale Minus - the final diff algorithm
* Downscale Diff 2x - Diff grayscale Minus downsamples 2x.
* Downscale Diff 3x - Diff grayscale Minus downsamples 3x.
* Downscale Diff 4x - Diff grayscale Minus downsamples 4x.
* Diff color abs - Experimental diff algorithm.
* Diff color Minus - Experimental diff algorithm.


### camera-capture_cpp
Program for capturing images and transferring them to the computer. Captures grayscale and diff images, including 2x, 3x, 4x scaled versions. 

To capture, run the Python script at `lib\python\receive_image.py` to capture images, for example `python .\receive_image.py COM4`.

The camera has several modes, with `button0` cycling through the modes.
* **LIVE** mode - initial mode
  + `Button0` goes to **Capture** mode
  + `Button1` toggles between showing grayscale and diff images on the screen
  + `Button2` cycles between various delays between the current and diff image.
* **Capture** mode - captures a still image. The captured image is frozen, and the display toggles repeatedly between grayscale and diff image until a button is pressed. 
  + `Button0` goes to **MOVIE LIVE** mode
  + `Button1` transfers the image to the computer labeled as a `drone` image, and goes back to **LIVE** mode when the transfer is complete.
  + `Button2` transfers the image to the computer labeled as a `clear` image, and goes back to **LIVE** mode when the transfer is complete.
* **MOVIE LIVE** mode - prepares to contiuously captures images.
  + `Button0` goes back to **LIVE** mode
  + `Button1` starts continuous capture of `drone` images, and goes to **Movie capture** mode.
  + `Button2` starts continuous capture of `clear` images, and goes to **Movie capture** mode.
* **Movie capture** mode - captures images continuously until `Button0` or `Button2` is pressed. It is possible to quickly press `Button0` to abort image transfer before the first image is transferred.
  + `Button0` ends capture and goes to **LIVE** mode.
  + `Button1` toggles between showing grayscale and diff images on the screen (while still capturing)
  + `Button2` ends capture and goes to **LIVE** mode.


### drone_inference_base
Infers if there is a drone or not in the image frame. The screen displays the camera input downscaled to 2x. If a drone is detected, a bounding box is drawn on the screen over the detected drone, displaying the inference confidence value.

Controls:
* `Button0` toggles between grayscale or diff image, both downscaled to 2x.
* `Button1` toggles if the program should display a confidence bounding box a drone is not found. If on, it draws a green bounding box around the screen displaying the confidence value.


### image-labeler ###
Program for labelling captured files. The easiest way to run it is to open the image-labeler folder in Visual Studio Code and compile and do `Run->Start Debugging` from there. May require installing the .Net SDK.

You can select desired files, label them and save the labeling information. There are keyboard shortcuts to repidly move through images, files can be marked as ignored, there is some simple functionality to batch label `clear` files, and some unfinished functionality to better organize the files.


