import serial
import numpy as np
import matplotlib.pyplot as plt
import soundfile as sf  # install with: pip install soundfile

import sys

enable_plot = "-p" in sys.argv        # Set False to skip plotting
wav_filename = "output.wav"


# Parameters
serial_port = '/dev/ttyUSB0'     # Change this to your serial port
baudrate = 460800
sample_rate = 40179  # Hz
duration_sec = 60         # How long to record

# Open serial
ser = serial.Serial(serial_port, baudrate, timeout=1)

# Calculate how many samples to collect
num_samples = int(sample_rate * duration_sec)

# Buffer for samples
samples = np.empty(num_samples, dtype=np.uint8)

# print(f"Recording {duration_sec} seconds of audio...")

# Setup plot
if enable_plot:
    plt.ion()
    fig, ax = plt.subplots()
    line, = ax.plot(np.zeros(500))
    ax.set_ylim(0, 255)
    ax.set_title("UART Audio Waveform (latest 500 samples)")
    ax.set_xlabel("Sample")
    ax.set_ylabel("Amplitude (0-255)")

sample_idx = 0
while sample_idx < num_samples:
    print(f"Samples received: {sample_idx}/{num_samples}", end='\r')
    if ser.in_waiting:
        # Read one byte
        data = ser.read(1)
        if data:
            samples[sample_idx] = data[0]
            sample_idx += 1

            # Update plot every 500 samples
            if enable_plot and sample_idx % 500 == 0:
                line.set_ydata(samples[max(0, sample_idx-500):sample_idx])
                line.set_xdata(np.arange(len(line.get_ydata())))
                ax.relim()
                ax.autoscale_view()
                plt.pause(0.01)

print("Done receiving. Closing serial port.")
ser.close()

# Convert to float [-1, 1] for WAV file
audio_float = (samples.astype(np.float32) - 128) / 128.0

# Save to WAV
sf.write(wav_filename, audio_float, samplerate=sample_rate)

print(f"WAV file saved: {wav_filename}")

if enable_plot:
    plt.ioff()
    plt.figure()
    plt.plot(audio_float)
    plt.title("Full Recorded Audio")
    plt.xlabel("Sample")
    plt.ylabel("Amplitude [-1, 1]")
    plt.grid()
    plt.show()
