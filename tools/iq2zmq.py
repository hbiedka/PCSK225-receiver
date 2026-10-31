import struct
import time
import serial
import zmq

# --- KONFIGURACJA ZMQ ---
context = zmq.Context()
socket = context.socket(zmq.PUB)
socket.bind("tcp://127.0.0.1:5555")

# --- KONFIGURACJA UART ---
PORT = "/dev/ttyACM0"
BAUD = 921600

# Musisz tu wpisać dokładnie tyle samo co AF_HALF_SAMPLES w STM32!
# (np. jeśli AF_HALF_SAMPLES = 250, to block_bytes = 250 * 4 = 1000)
AF_HALF_SAMPLES = (
    16
)  # <--- DOSTOSUJ DO SWOJEGO KODU W STM32 (liczba próbek w połowie bufora)
BLOCK_BYTES = AF_HALF_SAMPLES * 4

print(
    f"Uruchomiono odbiornik blokowy z {PORT} @ {BAUD} (Rozmiar bloku:"
    f" {AF_HALF_SAMPLES} próbek)..."
)

frame_count = 0
start_time = time.time()
last_print_time = start_time

try:
  with serial.Serial(PORT, BAUD, timeout=0.1) as ser:
    ser.reset_input_buffer()

    while True:
      # 1. Czekamy na globalny nagłówek 0xFF, 0xFF na początku bloku
      header = ser.read_until(b"\xff\xff")

      if header.endswith(b"\xff\xff"):
        # 2. Czytamy cały blok danych naraz
        block_data = ser.read(BLOCK_BYTES)

        if len(block_data) == BLOCK_BYTES:
          # Przetwarzamy próbki po 4 bajty wewnątrz bloku
          for i in range(0, BLOCK_BYTES, 4):
            i_val, q_val = struct.unpack(
                ">hh", block_data[i : i + 4]
            )

            # Normalizacja do GNU Radio
            i_f = i_val / 32768.0
            q_f = q_val / 32768.0

            # Wysłanie przez ZMQ
            payload = struct.pack("ff", i_f, q_f)
            socket.send(payload)
            frame_count += 1

      # Statystyki co sekundę
      current_time = time.time()
      elapsed = current_time - last_print_time
      if elapsed >= 0.1:
        total_elapsed = current_time - start_time
        fps = frame_count / elapsed
        print(
            f"{fps:.1f} samples/s              ",end='\r'
        )
        last_print_time = current_time
        frame_count = 0

except Exception as e:
  print(f"Błąd: {e}")