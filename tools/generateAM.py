import numpy as np
import matplotlib.pyplot as plt
import math

def generate_am_wave_c_array_uint16(sample_rate, carrier_freq, modulating_freq,
                                    num_samples, modulation_depth=0.5,
                                    amplitude=0.5, dc_offset=0.5):
    t = np.arange(num_samples) / sample_rate
    modulator = np.sin(2 * np.pi * modulating_freq * t)
    carrier = np.sin(2 * np.pi * carrier_freq * t)
    am_wave = (1 + modulation_depth * modulator) * carrier
    am_wave = am_wave * amplitude + dc_offset
    am_wave = np.clip(am_wave, 0, 1)
    samples_uint16 = np.round(am_wave * 4096).astype(np.uint16)
    return am_wave, samples_uint16


def iq_detector(samples, sample_rate, rf_freq, filter_ratio=0.1, chunk_size=None):
    """
    Simulate IQ detector demodulation.

    samples: normalized input samples (0 to 1)
    sample_rate: Hz
    rf_freq: carrier frequency (Hz)
    filter_ratio: IIR filter coefficient
    """
    # Remove DC offset and scale to [-1, 1]
    # rf_signal = samples - np.mean(samples)
    # rf_signal /= np.max(np.abs(rf_signal))
    rf_signal = samples

    # Number of samples per RF cycle
    samples_per_rf_cycle = sample_rate / rf_freq
    print(f"Samples per RF cycle: {samples_per_rf_cycle}")

    # Initialize I and Q values
    I_vals = []
    Q_vals = []
    abs_vals = []
    phase = []

    if chunk_size:

        lut_periods =  math.floor((chunk_size-(samples_per_rf_cycle))/ samples_per_rf_cycle)
        lut_len = int(samples_per_rf_cycle * lut_periods)
        print(f"LUT length: {lut_len}, LUT periods: {lut_periods}")

        #start with 0 by sample_per_rf_cycle, then sin by lut_len, next zeros
        sin_lut = np.zeros(chunk_size*2)
        cos_lut = np.zeros(chunk_size*2)

        sin_offset = int(samples_per_rf_cycle)
        # cos_offset = int(samples_per_rf_cycle*1.25)

        for i in range(lut_len):
            sin_lut[i+sin_offset] = 128 * np.sin(2 * np.pi * rf_freq * i / sample_rate)
            cos_lut[i+sin_offset] = 128 * np.cos(2 * np.pi * rf_freq * i / sample_rate)

        sin_lut = sin_lut.astype(np.int32)
        cos_lut = cos_lut.astype(np.int32)
            
        #write LUTs to header file
        write_c_header("sin_lut.h", "sin_lut", sin_lut.astype(np.int32), array_type="int32_t")
        write_c_header("cos_lut.h", "cos_lut", cos_lut.astype(np.int32), array_type="int32_t")

        output_sin_lut = []
        output_cos_lut = []


        # Emulate mixing with I and Q components
        lut_pos = 0
        i = 0
        while i < len(rf_signal):
            if i + chunk_size > len(rf_signal):
                break
            # segment = rf_signal[i:i + chunk_size]

            
            # Mixing with LUTs
            I = 0
            Q = 0

            if lut_pos >= chunk_size:
                # print(f"Warning: LUT position {lut_pos} exceeds chunk size.")
                lut_pos = int(lut_pos % samples_per_rf_cycle)
                # print(f"Resetting LUT position to {lut_pos}.")

            chunk_end = i + chunk_size

            while i < chunk_end:
                # print(f"LUT pos: {lut_pos}, j: {j}")
                I += rf_signal[i] * sin_lut[lut_pos]
                Q += rf_signal[i] * cos_lut[lut_pos]

                output_sin_lut.append(sin_lut[lut_pos]/128)
                output_cos_lut.append(cos_lut[lut_pos]/128)

                lut_pos += 1
                i += 1
                    
            I_vals.append(I)
            Q_vals.append(Q)
            abs_vals.append(np.sqrt(I ** 2 + Q ** 2))
            phase.append(np.arctan2(Q, I))

        #normalize output LUTs
        norm_output_sin_lut = np.array(output_sin_lut) / np.max(np.abs(output_sin_lut))
        norm_output_cos_lut = np.array(output_cos_lut) / np.max(np.abs(output_cos_lut))

        #norlmalize rf_signal
        norm_rf_signal = rf_signal-np.mean(rf_signal) 
        norm_rf_signal /= np.max(np.abs(norm_rf_signal))

        plt.figure(figsize=(10, 3))
        plt.plot(norm_output_sin_lut[:1000], label="I LUT", alpha=0.7)
        plt.plot(norm_output_cos_lut[:1000], label="Q LUT", alpha=0.7)
        plt.plot(norm_rf_signal[:1000], label="RF Signal", alpha=0.7)
        plt.title("LUTs and RF Signal")
        plt.xlabel("Sample index")
        plt.ylabel("Amplitude [0, 1]")
        plt.grid(True)
        plt.legend()

        # plt.show()

        
    else:
        #continous mode

        # Emulate mixing with LO
        for i in range(0, len(rf_signal), int(samples_per_rf_cycle)):
            if i + int(samples_per_rf_cycle) > len(rf_signal):
                break
            segment = rf_signal[i:i + int(samples_per_rf_cycle)]
            
            I = np.mean(segment * np.cos(2 * np.pi * rf_freq * np.arange(len(segment)) / sample_rate))
            Q = np.mean(segment * np.sin(2 * np.pi * rf_freq * np.arange(len(segment)) / sample_rate))
                    
            I_vals.append(I)
            Q_vals.append(Q)
            abs_vals.append(np.sqrt(I ** 2 + Q ** 2))
            phase.append(np.arctan2(Q, I))

    return np.array(I_vals), np.array(Q_vals), np.array(abs_vals), np.array(phase)

def dpsk(wave, period,dc_offset=0.5):
    """    
        Differential Phase Shift Keying (DPSK) modulation.
    """

    cnt = 0
    phase = 1
    for i in range(0, len(wave)):
        wave[i] -= dc_offset
        wave[i] *= phase
        wave[i] += dc_offset

        cnt += 1
        if cnt >= period:
            cnt = 0
            phase = -phase

    return wave

def write_c_header(filename, array_name, samples, array_type="uint16_t"):
    with open(filename, "w") as f:
        f.write("#ifndef " + array_name.upper() + "_H\n")
        f.write("#define " + array_name.upper() + "_H\n\n")
        f.write("#include <stdint.h>\n\n")
        f.write(f"#define {array_name.upper()}_SIZE {len(samples)}\n\n")
        f.write(f"{array_type} {array_name}[{len(samples)}] = {{\n")
        for i, val in enumerate(samples):
            if i % 8 == 0:
                f.write("    ")
            f.write(f"{val}, ")
            if (i + 1) % 8 == 0:
                f.write("\n")
        if len(samples) % 8 != 0:
            f.write("\n")
        f.write("};\n\n")
        f.write("#endif //" + array_name.upper() + "_H\n")

if __name__ == "__main__":
    # Parameters
    sample_rate = 2250000      # Hz
    carrier_freq = 225000      # Hz
    modulating_freq = 1099     # Hz
    num_samples = 8192  # Number of samples to generate
    modulation_depth = 0.3
    amplitude = 0.02
    dc_offset = 0.5

    array_name = "am_wave"
    header_filename = "am_wave.h"

    # Generate waveform
    am_wave, samples = generate_am_wave_c_array_uint16(
        sample_rate, carrier_freq, modulating_freq,
        num_samples, modulation_depth, amplitude, dc_offset
    )

    # am_wave = dpsk(am_wave, num_samples/10, dc_offset=dc_offset)

    # Write header
    write_c_header(header_filename, array_name, samples)
    print(f"Header file '{header_filename}' generated with array '{array_name}'.")

    # Demodulate
    I_vals, Q_vals, abs_vals, phase = iq_detector(am_wave*4096, sample_rate, carrier_freq,chunk_size=64)
    
    # Plot original AM wave
    plt.figure(figsize=(10, 3))
    plt.plot(am_wave, label="AM Wave (Normalized)", alpha=0.7)
    plt.title("AM Waveform")
    plt.xlabel("Sample index")
    plt.ylabel("Amplitude [0, 1]")
    plt.grid(True)
    plt.legend()

    # Plot I and Q
    plt.figure(figsize=(10, 3))
    plt.plot(I_vals, label="I", alpha=0.7)
    plt.plot(Q_vals, label="Q", alpha=0.7)
    plt.title("I/Q Components")
    plt.xlabel("Output sample index")
    plt.ylabel("Amplitude (relative)")
    plt.grid(True)
    plt.legend()

    print(I_vals)

    # Plot demodulated envelope
    plt.figure(figsize=(10, 3))
    plt.plot(abs_vals, label="Demodulated Envelope", color="purple")
    plt.title("Demodulated AM Envelope (Audio)")
    plt.xlabel("Output sample index")
    plt.ylabel("Amplitude (relative)")
    plt.grid(True)
    plt.legend()

    # Plot phase
    plt.figure(figsize=(10, 3))
    plt.plot(phase*(180/np.pi), label="Phase", color="orange")
    plt.title("Phase of Demodulated Signal")
    plt.xlabel("Output sample index")
    plt.ylabel("Phase (degrees)")
    plt.grid(True)
    plt.legend()


    plt.tight_layout()
    plt.show()
