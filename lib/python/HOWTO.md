# How to Use receive_image.py

Receives images sent over UART from an nRF54L15 board and saves them as PNG files.

## Requirements

```
pip install pyserial pillow
```

NumPy is optional but recommended for faster RGB565 decoding:

```
pip install numpy
```

## Basic Usage

```
python receive_image.py PORT
```

**Examples:**

```
python receive_image.py COM4
python receive_image.py /dev/ttyACM0
```

Images are saved to a `received_images/` folder next to the script.

## Options

| Argument | Description | Default |
|---|---|---|
| `PORT` | Serial port to listen on | *(required)* |
| `--folder FOLDER` | Base output folder | `train/dataset/` next to script |
| `--filename NAME` | Fallback filename if the frame sends none | `frame_NNNN_TIMESTAMP.png` |

## Output Folder Structure

```
received_images/
  [label]/        ← created only if the frame includes a label
    filename.png
```

Values sent in the frame metadata (`label=`, `folder=`, `filename=`) always take precedence over the CLI arguments.

## Stopping

Press **Ctrl+C** to stop.

## Troubleshooting

- **`pyserial not found`** — run `pip install pyserial`
- **`Pillow not found`** — run `pip install pillow`
- **Serial error** — check that the correct port is specified and no other program (e.g. a terminal emulator) is using it
- **Truncated frames / NACK messages** — check the USB cable and that the board is running the correct firmware
