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


def iq_detector(samples, sample_rate, rf_freq, chunk_size=64):
    """
    Simulate IQ detector demodulation.

    samples: normalized input samples (0 to 1)
    sample_rate: Hz
    rf_freq: carrier frequency (Hz)
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

    lut_periods =  math.floor((chunk_size-(samples_per_rf_cycle))/ samples_per_rf_cycle)
    lut_len = int(samples_per_rf_cycle * lut_periods)
    print(f"LUT length: {lut_len}, LUT periods: {lut_periods}")

    #start with 0 by sample_per_rf_cycle, then sin by lut_len, next zeros
    sin_lut = np.zeros(chunk_size*2)
    cos_lut = np.zeros(chunk_size*2)

    sin_offset = int(samples_per_rf_cycle)

    for i in range(lut_len):
        sin_lut[i+sin_offset] = 128 * np.sin(2 * np.pi * rf_freq * i / sample_rate)
        cos_lut[i+sin_offset] = 128 * np.cos(2 * np.pi * rf_freq * i / sample_rate)


    sin_lut = sin_lut.astype(np.int32)
    cos_lut = cos_lut.astype(np.int32)
        
    #write LUTs to header file
    write_c_header("sin_lut.h", "sin_lut", sin_lut, array_type="int32_t")
    write_c_header("cos_lut.h", "cos_lut", cos_lut, array_type="int32_t")

    output_sin_lut = []
    output_cos_lut = []


    # Emulate mixing with I and Q components
    lut_pos = 0
    i = 0

    # Process in chunks
    while i < len(rf_signal):
        if i + chunk_size > len(rf_signal):
            break
        
        # Mixing with LUTs
        I = 0
        Q = 0

        if lut_pos >= chunk_size:
            lut_pos = int(lut_pos % samples_per_rf_cycle)

        chunk_end = i + chunk_size

        while i < chunk_end:
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

    return np.array(I_vals), np.array(Q_vals), np.array(abs_vals), np.array(phase)

def generate_LUT(sample_rate, rf_freq, max_len=1024):
    sin_lut = np.zeros(max_len)

    for i in range(max_len):
        sin_lut[i] = np.sin(2 * np.pi * rf_freq * i / sample_rate)

    # find the best fit length
    i = math.floor(sample_rate / rf_freq)
    best_fit_len = i

    while i < max_len:
        if abs(sin_lut[i]) < abs(sin_lut[best_fit_len]):
            best_fit_len = i
        i += 1

    print(f"Best fit length: {best_fit_len} with value {sin_lut[best_fit_len]}")
    sin_lut = sin_lut[:best_fit_len]

    return sin_lut

def simple_detector(samples, sample_rate, rf_freq):

    rf_signal = samples

    # Number of samples per RF cycle
    samples_per_rf_cycle = sample_rate / rf_freq
    print(f"Samples per RF cycle: {samples_per_rf_cycle}")

    # Initialize I and Q values
    I_vals = []
    Q_vals = []
    abs_vals = []
    phase = []

    sin_lut = np.zeros(len(rf_signal))


    # for i in range(len(rf_signal)):
    #     sin_lut[i] = 128 * np.sin(2 * np.pi * rf_freq * i / sample_rate)

    sin_lut = 128*generate_LUT(sample_rate, rf_freq)
    sin_lut = sin_lut.astype(np.int32)
        
    #write LUTs to header file
    write_c_header("af_sin_lut.h", "af_sin_lut", sin_lut, array_type="int32_t")

    # Emulate mixing
    i = 0
    lut_pos = 0
    # Process the entire signal
    while i < len(rf_signal):

        # Mixing with LUTs
        I = rf_signal[i] * sin_lut[lut_pos]
        i += 1
        lut_pos += 1
        if lut_pos >= len(sin_lut):
            lut_pos = 0
        
        I_vals.append(I)

    return np.array(I_vals)

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
    sample_rate = 2571429      # Hz
    carrier_freq = 225000      
    modulating_freq = 1099     # Hz
    num_samples = 200000  # Number of samples to generate
    modulation_depth = 0.3
    amplitude = 0.02
    dc_offset = 0.5

    print(f"carrier_freq: {carrier_freq} Hz")

    array_name = "am_wave"
    header_filename = "am_wave.h"

    # Generate waveform
    am_wave, samples = generate_am_wave_c_array_uint16(
        sample_rate, carrier_freq, modulating_freq,
        num_samples, modulation_depth, amplitude, dc_offset
    )

    #4600 is close to 55 bps
    am_wave = dpsk(am_wave, 46000, dc_offset=dc_offset)

    # Write header
    write_c_header(header_filename, array_name, samples)
    print(f"Header file '{header_filename}' generated with array '{array_name}'.")

    # Demodulate to IF
    if_freq = sample_rate / math.floor(sample_rate / carrier_freq)
    print(f"IF frequency: {if_freq} Hz")

    # chunk size
    chunk_size = 64  # Number of samples per chunk for demodulation

    I_vals, Q_vals, abs_vals, phase = iq_detector(am_wave*4096, sample_rate, if_freq, chunk_size=chunk_size)

    #calculate new sample rate
    if_sample_rate = sample_rate / chunk_size
    print(f"IF sample rate: {if_sample_rate} Hz")
    
    #calculate residual frequency
    residual_freq = if_freq - carrier_freq
    print(f"Residual frequency: {residual_freq} Hz")
    
    # samples_per_if_cycle = math.floor(if_sample_rate / residual_freq)
    # real_residual_freq = if_sample_rate / samples_per_if_cycle
    # print(f"Real residual frequency: {real_residual_freq} Hz")

    # print(f"Real demodulated frequency: {if_freq-real_residual_freq} Hz")

    

    af_i = simple_detector(I_vals, if_sample_rate, residual_freq)
    af_q = simple_detector(Q_vals, if_sample_rate, residual_freq)

    # filter I and Q components
    for i in range(1,len(af_i)):
        af_i[i] = af_i[i-1] * 0.9 + af_i[i] * 0.1
        af_q[i] = af_q[i-1] * 0.9 + af_q[i] * 0.1

    # demodulate to audio
    af_abs_vals = np.sqrt(af_i**2 + af_q**2)
    af_phase = np.arctan2(af_q, af_i)

    #filter audio envelope
    for i in range(1, len(af_abs_vals)):
        af_abs_vals[i] = af_abs_vals[i-1] * 0.9 + af_abs_vals[i] * 0.1

    fig = plt.figure(figsize=(12, 10))
    gs = fig.add_gridspec(4, 2, height_ratios=[1, 1, 1, 1])

    # Row 1: AM waveform spanning both columns
    ax_am = fig.add_subplot(gs[0, :])
    ax_am.plot(am_wave, label="AM Wave (Normalized)", alpha=0.7)
    ax_am.set_title("AM Waveform")
    ax_am.set_xlabel("Sample index")
    ax_am.set_ylabel("Amplitude [0, 1]")
    ax_am.grid(True)
    ax_am.legend()

    # Row 2: IF I/Q (left) and AF I/Q (right)
    ax_if_iq = fig.add_subplot(gs[1, 0])
    ax_if_iq.plot(I_vals, label="I", alpha=0.7)
    ax_if_iq.plot(Q_vals, label="Q", alpha=0.7)
    ax_if_iq.set_title("IF I/Q Components")
    ax_if_iq.set_xlabel("Output sample index")
    ax_if_iq.set_ylabel("Amplitude (relative)")
    ax_if_iq.grid(True)
    ax_if_iq.legend()

    ax_af_iq = fig.add_subplot(gs[1, 1])
    ax_af_iq.plot(af_i, label="AF I", alpha=0.7)
    ax_af_iq.plot(af_q, label="AF Q", alpha=0.7)
    ax_af_iq.set_title("AF I/Q Components")
    ax_af_iq.set_xlabel("Output sample index")
    ax_af_iq.set_ylabel("Amplitude (relative)")
    ax_af_iq.grid(True)
    ax_af_iq.legend()

    # Row 3: IF amplitude (left) and AF amplitude (right)
    ax_if_amp = fig.add_subplot(gs[2, 0])
    ax_if_amp.plot(abs_vals, label="IF Demodulated Envelope", color="purple")
    ax_if_amp.set_title("Demodulated IF Envelope")
    ax_if_amp.set_xlabel("Output sample index")
    ax_if_amp.set_ylabel("Amplitude (relative)")
    ax_if_amp.grid(True)
    ax_if_amp.legend()

    ax_af_amp = fig.add_subplot(gs[2, 1])
    ax_af_amp.plot(af_abs_vals, label="Demodulated Envelope", color="purple", alpha=0.7)
    ax_af_amp.set_title("Demodulated AM Envelope (Audio)")
    ax_af_amp.set_xlabel("Output sample index")
    ax_af_amp.set_ylabel("Amplitude (relative)")
    ax_af_amp.grid(True)
    ax_af_amp.legend()

    # Row 4: IF phase (left) and AF phase (right)
    ax_if_phase = fig.add_subplot(gs[3, 0])
    ax_if_phase.plot(phase*(180/np.pi), label="Phase", color="orange")
    ax_if_phase.set_title("Phase of Demodulated Signal (IF)")
    ax_if_phase.set_xlabel("Output sample index")
    ax_if_phase.set_ylabel("Phase (degrees)")
    ax_if_phase.grid(True)
    ax_if_phase.legend()

    ax_af_phase = fig.add_subplot(gs[3, 1])
    ax_af_phase.plot(af_phase*(180/np.pi), label="Phase", color="orange", alpha=0.7)
    ax_af_phase.set_title("Phase of Demodulated Signal (AF)")
    ax_af_phase.set_xlabel("Output sample index")
    ax_af_phase.set_ylabel("Phase (degrees)")
    ax_af_phase.grid(True)
    ax_af_phase.legend()

    plt.tight_layout()
    plt.show()