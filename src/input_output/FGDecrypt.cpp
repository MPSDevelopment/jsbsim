/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

 Module:       FGDecrypt.cpp
 Author:       JSBSim Team
 Date started: 01/08/26
 Purpose:      AES-256 decryption for encrypted aircraft XML files

 ------------- Copyright (C) 2026 JSBSim Team -------------

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU Lesser General Public License as published by the Free Software
 Foundation; either version 2 of the License, or (at your option) any later
 version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 details.

 You should have received a copy of the GNU Lesser General Public License along with
 this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 Place - Suite 330, Boston, MA  02111-1307, USA.

 Further information about the GNU Lesser General Public License can also be found on
 the world wide web at http://www.gnu.org.

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
INCLUDES
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#include "FGDecrypt.h"
#include <fstream>
#include <iostream>
#include <cstring>

#include <openssl/evp.h>
#include <openssl/aes.h>

namespace JSBSim {

/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
STATIC DATA
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

// AES-256 key (32 bytes) - CHANGE THIS KEY FOR YOUR DISTRIBUTION
static const unsigned char AES_KEY[32] = {
  0x4a, 0x53, 0x42, 0x53, 0x69, 0x6d, 0x41, 0x45,  // "JSBSimAE"
  0x53, 0x32, 0x35, 0x36, 0x4b, 0x65, 0x79, 0x21,  // "S256Key!"
  0x46, 0x6c, 0x69, 0x67, 0x68, 0x74, 0x44, 0x79,  // "FlightDy"
  0x6e, 0x61, 0x6d, 0x69, 0x63, 0x73, 0x21, 0x21   // "namics!!"
};

/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
CLASS IMPLEMENTATION
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

std::string FGDecrypt::Decrypt(const std::vector<unsigned char>& encrypted_data)
{
  if (encrypted_data.size() < 17) {
    std::cerr << "FGDecrypt: Encrypted data too short" << std::endl;
    return "";
  }

  // First 16 bytes are the IV
  const unsigned char* iv = encrypted_data.data();
  const unsigned char* ciphertext = encrypted_data.data() + 16;
  int ciphertext_len = static_cast<int>(encrypted_data.size() - 16);

  // Allocate output buffer (plaintext will be smaller or equal to ciphertext)
  std::vector<unsigned char> plaintext(ciphertext_len + AES_BLOCK_SIZE);
  int plaintext_len = 0;
  int len = 0;

  // Create and initialize the context
  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  if (!ctx) {
    std::cerr << "FGDecrypt: Failed to create cipher context" << std::endl;
    return "";
  }

  // Initialize decryption operation with AES-256-CBC
  if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, AES_KEY, iv) != 1) {
    std::cerr << "FGDecrypt: Failed to initialize decryption" << std::endl;
    EVP_CIPHER_CTX_free(ctx);
    return "";
  }

  // Decrypt the ciphertext
  if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, ciphertext, ciphertext_len) != 1) {
    std::cerr << "FGDecrypt: Decryption failed" << std::endl;
    EVP_CIPHER_CTX_free(ctx);
    return "";
  }
  plaintext_len = len;

  // Finalize decryption (handles padding)
  if (EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len) != 1) {
    std::cerr << "FGDecrypt: Decryption finalization failed (wrong key or corrupted data)" << std::endl;
    EVP_CIPHER_CTX_free(ctx);
    return "";
  }
  plaintext_len += len;

  EVP_CIPHER_CTX_free(ctx);

  return std::string(reinterpret_cast<char*>(plaintext.data()), plaintext_len);
}

std::vector<unsigned char> FGDecrypt::ReadEncryptedFile(const SGPath& path)
{
  std::vector<unsigned char> contents;

  std::ifstream file(path.utf8Str(), std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    return contents;
  }

  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  contents.resize(static_cast<size_t>(size));
  if (!file.read(reinterpret_cast<char*>(contents.data()), size)) {
    contents.clear();
  }

  return contents;
}

SGPath FGDecrypt::GetEncryptedPath(const SGPath& path)
{
  SGPath encPath(path);
  encPath.concat(".enc");

  if (encPath.exists()) {
    return encPath;
  }

  return SGPath();
}

} // namespace JSBSim
