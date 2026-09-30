import numpy as np
import matplotlib.pyplot as plt
import math

def generate_modulating_signal(sample_rate, modulating_freq, num_samples):
    """
        Generates sine wave audio signal [-1, 1].
    """
    t = np.arange(num_samples) / sample_rate
    return np.sin(2 * np.pi * modulating_freq * t)

def generate_square_phase_signal(sample_rate, num_samples, phase_shift_rad=np.pi/2, phase_offset=0, toggle_freq=1.0):
    """
        Generates square phase change
    """
    t = np.arange(num_samples) / sample_rate

    square_wave = (np.sin(2 * np.pi * toggle_freq * t) >= 0).astype(float)
    return square_wave * phase_shift_rad + phase_offset

def generate_am_wave(
        sample_rate,
        carrier_freq,
        audio_signal,
        phase_signal,
        modulation_depth=0.5, amplitude=0.5, dc_offset=0.0):
    """
        Generates AM wave modulated by audio_signal and carrier phase modulated
        by phase_signal
    """
    num_samples = len(audio_signal)

    t = np.arange(num_samples) / sample_rate

    # Carrier with phase modulated
    carrier = np.sin(2 * np.pi * carrier_freq * t + phase_signal)

    # Amplitude modulation
    am_wave = (1 + modulation_depth * audio_signal) * carrier

    # Add amplitude and DC offset
    am_wave = am_wave * amplitude + dc_offset
    return am_wave

def convert_float_to_int16(signal_float, max_val_clamp=1.0, res=2048):
    """
    Converts float [-max_val_clamp, max_val_clamp]
    to int16 (-32767 do 32767).
    """

    clipped_signal = np.clip(signal_float, -max_val_clamp, max_val_clamp)
    scaled_samples = np.round((clipped_signal / max_val_clamp) * float(res))
    return scaled_samples.astype(np.int16)


def generate_nco_luts(lut_bits=8):
    lut_size = 1 << lut_bits  # 256
    indices = np.arange(lut_size)

    # Wartości w zakresie -127 .. 127 dla int8_t
    sin_lut = np.round(127 * np.sin(2 * np.pi * indices / lut_size)).astype(
        np.int8
    )
    cos_lut = np.round(127 * np.cos(2 * np.pi * indices / lut_size)).astype(
        np.int8
    )

    write_c_header("sin_lut.h", "sin_lut", sin_lut, array_type="int8_t")
    write_c_header("cos_lut.h", "cos_lut", cos_lut, array_type="int8_t")

    return sin_lut, cos_lut

def generate_atan_lut(lut_bits=8,angle_scale=2**32):
    lut_size = 1 << lut_bits  # 256 elements

    # Create arcus tangens in range 0-1 (which converts to 0-pi/4)
    x = np.linspace(0.0, 1.0, lut_size + 1)
    angles_rad = np.arctan(x)

    # Convert to binary radian (2pi = 65536)
    scale_factor = float(angle_scale) / (2.0 * np.pi)
    atan_lut = np.round(angles_rad * scale_factor)

    write_c_header("atan_lut.h","atan_lut",atan_lut.astype(int),array_type="uint16_t")
    return atan_lut

def fast_atan2(y,x,atan_lut,angle_scale=2**32):

    # angle coefficients
    TWO_PI = angle_scale
    PI = angle_scale/2
    HALF_PI = angle_scale/4

    # LUT size without last index -> equal to 2^lut_bits
    lut_size = len(atan_lut)-1

    if x == 0 and y == 0:
        return 0

    abs_y = abs(y)
    abs_x = abs(x)

    if abs_y > abs_x:
        index = int(abs_x*lut_size/abs_y)
        angle = atan_lut[index]

        # swap to pi/2 - angle
        angle = HALF_PI-angle
    else:
        # no swap
        index = int(abs_y*lut_size/abs_x)
        angle = atan_lut[index]

    #quadrant correction
    if x < 0:
        if y < 0:
            angle = PI + angle   # 3rd quadrant
        else:
            angle = PI - angle   # 2nd quadrant
    else:
        if y < 0:
            angle = TWO_PI - angle   # 4th quadrant


    # emulate integer rollover
    angle %= TWO_PI

    return angle

def nco_iq_mix(rf_signal, sample_rate, rf_freq, downsample=64):

    lut_bits = 8
    acc_bits = 32

    # generate LUTs
    sin_lut, cos_lut = generate_nco_luts(lut_bits)

    #calculate phase increment
    phase_ac_incr = int((rf_freq/sample_rate)*(2**acc_bits))

    #calculate phase bitcut to emulate unit32 overload (don't needed in target)
    phase_ac_bitcut = (2**acc_bits)-1

    #calculate phase accumulator shift
    phase_ac_bitshift = acc_bits - lut_bits

    i = 0
    phase_ac = 0
    I_samples = []
    Q_samples = []

    while i < num_samples - downsample:
        I_sum = 0
        Q_sum = 0

        for _ in range(downsample):
            lut_index = phase_ac >> phase_ac_bitshift

            I_sum += rf_signal[i] * sin_lut[lut_index]
            Q_sum += rf_signal[i] * cos_lut[lut_index]

            phase_ac += phase_ac_incr

            #Don't need this in target
            phase_ac &= phase_ac_bitcut

            i+=1

        I_samples.append(I_sum)
        Q_samples.append(Q_sum)

    return np.array(I_samples), np.array(Q_samples)

def write_c_header(filename, array_name, samples, array_type="int16_t"):
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

    # SDR parameters
    downsample = 64     # decimation ratio from RF to AF
    ssb_tuneoff = 1000  # tuneoff from carrier to generate SSB-like signal to phase detection
    ref_osc_drift = 10  # drift between demodulated carrier and suboscillator

    # Parameters
    sample_rate = 2571429      # RF sample rate [Hz]
    carrier_freq = 225000      # Carrier frequency [Hz]
    modulating_freq = 1099     # Emulated sine wave frequency [Hz]
    num_samples = int(50*sample_rate/modulating_freq)  # Number of samples to generate
    modulation_depth = 0.3
    amplitude = 0.02
    dc_offset = 0.5
    phase_modulating_freq = 75  # DPSK modulation [Hz]

    print(f"carrier_freq: {carrier_freq} Hz")

    array_name = "am_wave"
    header_filename = "am_wave.h"

    #generate arcus tangens LUT
    atan_lut = generate_atan_lut()
    fast_atan2_vec = np.vectorize(fast_atan2, excluded=['atan_lut'])

    # Generate waveform
    audio = generate_modulating_signal(sample_rate, modulating_freq, num_samples)
    phase = generate_square_phase_signal(sample_rate, num_samples, toggle_freq=phase_modulating_freq, phase_shift_rad=np.pi, phase_offset=-np.pi/2)

    am_wave = generate_am_wave(sample_rate, carrier_freq,
                               audio_signal=audio,
                               phase_signal=phase,
                               modulation_depth=modulation_depth,
                               amplitude=amplitude,
                               dc_offset=dc_offset
                               )

    #add white noise
    noise = np.random.normal(0, 0.1, num_samples)
    am_wave += noise

    rf_samples = convert_float_to_int16(am_wave)

    # Write header
    write_c_header(header_filename, array_name, rf_samples)
    print(f"Header file '{header_filename}' generated with array '{array_name}'.")

    af_sample_rate = sample_rate/downsample
    af_i,af_q = nco_iq_mix(rf_samples.astype(int), sample_rate, carrier_freq, downsample)

    print(f"AF sample rate {af_sample_rate} Hz")

    # filter I and Q components
    for i in range(1,len(af_i)):
        af_i[i] = af_i[i-1] * 0.9 + af_i[i] * 0.1
        af_q[i] = af_q[i-1] * 0.9 + af_q[i] * 0.1

    # demodulate to audio
    af_abs_vals = np.sqrt(af_i**2 + af_q**2)
    af_phase = fast_atan2_vec(af_q, af_i, atan_lut=atan_lut)*(np.pi*2/(2**32))

    #filter audio envelope
    for i in range(1, len(af_abs_vals)):
        af_abs_vals[i] = af_abs_vals[i-1] * 0.9 + af_abs_vals[i] * 0.1

    ssb_i,ssb_q = nco_iq_mix(rf_samples.astype(int), sample_rate, carrier_freq-ssb_tuneoff, downsample)

    # filter I and Q components
    for i in range(1,len(ssb_i)):
        ssb_i[i] = ssb_i[i-1] * 0.9 + ssb_i[i] * 0.1
        ssb_q[i] = ssb_q[i-1] * 0.9 + ssb_q[i] * 0.1

    raw_phase = fast_atan2_vec(ssb_q, ssb_i, atan_lut=atan_lut)*(np.pi*2/(2**32))

    # create reference phase to compare with phase modulated SSB single
    t = np.arange(len(ssb_i)) / af_sample_rate
    reference_carrier_phase = 2 * np.pi * (ssb_tuneoff+ref_osc_drift) * t
    ssb_phase = np.unwrap(raw_phase) - reference_carrier_phase

    ssb_phase = ssb_phase % (2 * np.pi)

    fig2 = plt.figure(figsize=(10, 3))
    gs2 = fig2.add_gridspec(2,1)
    ax_ssb = fig2.add_subplot(gs2[0,0])
    ax_ssb.plot(ssb_i, label="I", alpha=0.7)
    ax_ssb.plot(ssb_q, label="Q", alpha=0.7)
    ax_ssb.set_title("SSB Demodulated Signal")
    ax_ssb.set_xlabel("Sample index")
    ax_ssb.set_ylabel("Amplitude (relative)")
    ax_ssb.grid(True)
    ax_ssb.legend()

    ax_ssb_phase = fig2.add_subplot(gs2[1, 0])
    ax_ssb_phase.plot(raw_phase*(180/np.pi), label="Phase of demodulated carrier", alpha=0.7)
    ax_ssb_phase.plot(ssb_phase*(180/np.pi), label="Phase diff between carrier and reference", alpha=0.7)
    ax_ssb_phase.set_title("Phase of SSB Signal (AF)")
    ax_ssb_phase.set_xlabel("Output sample index")
    ax_ssb_phase.set_ylabel("Phase (degrees)")
    ax_ssb_phase.grid(True)
    ax_ssb_phase.legend()

    fig = plt.figure(figsize=(12, 10))
    gs = fig.add_gridspec(4, 1, height_ratios=[1, 1, 1, 1])

    # Row 1: AM waveform spanning both columns
    ax_am = fig.add_subplot(gs[0, 0])
    ax_am.plot(am_wave, label="AM Wave (Normalized)", alpha=0.7)
    ax_am.set_title("AM Waveform")
    ax_am.set_xlabel("Sample index")
    ax_am.set_ylabel("Amplitude [0, 1]")
    ax_am.grid(True)
    ax_am.legend()

    # Row 2: AF I/Q
    ax_af_iq = fig.add_subplot(gs[1, 0])
    ax_af_iq.plot(af_i, label="AF I", alpha=0.7)
    ax_af_iq.plot(af_q, label="AF Q", alpha=0.7)
    ax_af_iq.set_title("AF I/Q Components")
    ax_af_iq.set_xlabel("Output sample index")
    ax_af_iq.set_ylabel("Amplitude (relative)")
    ax_af_iq.grid(True)
    ax_af_iq.legend()

    # Row 3: AF amplitude
    ax_af_amp = fig.add_subplot(gs[2, 0])
    ax_af_amp.plot(af_abs_vals, label="Demodulated Envelope", color="purple", alpha=0.7)
    ax_af_amp.set_title("Demodulated AM Envelope (Audio)")
    ax_af_amp.set_xlabel("Output sample index")
    ax_af_amp.set_ylabel("Amplitude (relative)")
    ax_af_amp.grid(True)
    ax_af_amp.legend()

    # Row 4: AF phase
    ax_af_phase = fig.add_subplot(gs[3, 0])
    ax_af_phase.plot(af_phase*(180/np.pi), label="Phase", color="orange", alpha=0.7)
    ax_af_phase.set_title("Phase of Demodulated Signal (AF)")
    ax_af_phase.set_xlabel("Output sample index")
    ax_af_phase.set_ylabel("Phase (degrees)")
    ax_af_phase.grid(True)
    ax_af_phase.legend()

    plt.tight_layout()
    plt.show()