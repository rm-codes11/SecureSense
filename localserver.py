
from flask import Flask, request
from Crypto.Cipher import AES
from Crypto.Hash import HMAC, SHA256
import binascii
import json
import os


app = Flask(__name__)

# replace with your own keys
AES_KEY = b'this_is_a_32_byte_key_for_aes_256!!'  # 32 bytes
HMAC_KEY = b'this_is_another_32_byte_secret!!'    # 32 bytes

def decrypt_aes_cbc(encrypted_payload):
    try:
        # 1. Extract IV (first 32 hex chars = 16 bytes)
        iv_hex = encrypted_payload[:32]
        iv = binascii.unhexlify(iv_hex)
        
        # 2. Extract ciphertext (remaining except last 64 chars = HMAC)
        ciphertext_hex = encrypted_payload[32:-64]
        ciphertext = binascii.unhexlify(ciphertext_hex)
        
        # 3. Extract HMAC (last 64 hex chars = 32 bytes)
        received_hmac_hex = encrypted_payload[-64:]
        received_hmac = binascii.unhexlify(received_hmac_hex)
        
        # 4. Verify HMAC
        hmac_calculated = HMAC.new(HMAC_KEY, digestmod=SHA256)
        hmac_calculated.update(iv + ciphertext)
        if not hmac_calculated.digest() == received_hmac:
            return "[ERROR] HMAC verification failed"

        # 5. Decrypt (AES-256-CBC)
        cipher = AES.new(AES_KEY, AES.MODE_CBC, iv=iv)
        decrypted = cipher.decrypt(ciphertext)
        
        # 6. Remove PKCS#7 padding
        pad_length = decrypted[-1]
        if pad_length > AES.block_size:
            return "[ERROR] Invalid padding"
        decrypted = decrypted[:-pad_length]
        
        return decrypted.decode('utf-8')
    
    except Exception as e:
        return f"[ERROR] {str(e)}"

@app.route('/upload', methods=['POST'])
def receive_data():
    encrypted_payload = request.data.decode().strip()
    print("Received payload:", encrypted_payload)
    
    # Validate payload length (IV16 + ciphertext + HMAC32)
    if len(encrypted_payload) < 96:  # Minimum: 16B IV + 16B ciphertext + 32B HMAC
        return "Invalid payload length", 400
    
    decrypted = decrypt_aes_cbc(encrypted_payload)
    
    if decrypted.startswith("[ERROR]"):
        print("Decryption failed:", decrypted)
        return "Decryption error", 400
    
    try:
        # Validate JSON structure
        data = json.loads(decrypted)
        print("Decrypted data:", data)
        
        # Log to file
        with open("iot_log.txt", "a") as f:
            f.write(decrypted + "\n")
            
        return "Data processed", 200
        
    except json.JSONDecodeError:
        return "Invalid JSON after decryption", 400

if __name__ == "__main__":
    app.run(host='0.0.0.0', port=5000, ssl_context='adhoc')  # HTTPS 
