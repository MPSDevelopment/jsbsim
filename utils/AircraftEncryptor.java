import javax.crypto.Cipher;
import javax.crypto.spec.IvParameterSpec;
import javax.crypto.spec.SecretKeySpec;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.security.SecureRandom;

/**
 * Encrypts aircraft XML files for JSBSim using AES-256-CBC.
 *
 * Output format: [16 bytes IV][encrypted data]
 * Compatible with JSBSim FGDecrypt class.
 *
 * Input: aircraft.xml -> Output: aircraft.bin
 *
 * Usage: java AircraftEncryptor <input.xml> [output.bin]
 */
public class AircraftEncryptor {

    // AES-256 key (32 bytes) - MUST match key in FGDecrypt.cpp
    private static final byte[] AES_KEY = {
        (byte)0xc7, (byte)0xa1, (byte)0x38, (byte)0x80, (byte)0x09, (byte)0xf7, (byte)0x5e, (byte)0xb7,
        (byte)0x83, (byte)0xe6, (byte)0x5c, (byte)0x4b, (byte)0x4c, (byte)0x77, (byte)0x15, (byte)0x85,
        (byte)0xc2, (byte)0x22, (byte)0xc0, (byte)0x19, (byte)0xa3, (byte)0xfc, (byte)0x0f, (byte)0x30,
        (byte)0xe8, (byte)0x82, (byte)0x45, (byte)0x68, (byte)0xb9, (byte)0x47, (byte)0x31, (byte)0xed
    };

    public static void main(String[] args) {
        if (args.length < 1) {
            System.out.println("Usage: java AircraftEncryptor <input.xml> [output.bin]");
            System.out.println();
            System.out.println("Encrypts aircraft XML files for JSBSim using AES-256-CBC.");
            System.out.println("If output is not specified, replaces .xml with .bin");
            System.exit(1);
        }

        String inputFile = args[0];
        String outputFile;
        if (args.length > 1) {
            outputFile = args[1];
        } else {
            // Replace .xml with .bin
            if (inputFile.endsWith(".xml")) {
                outputFile = inputFile.substring(0, inputFile.length() - 4) + ".bin";
            } else {
                outputFile = inputFile + ".bin";
            }
        }

        try {
            encrypt(inputFile, outputFile);
            System.out.println("Encrypted: " + inputFile + " -> " + outputFile);
        } catch (Exception e) {
            System.err.println("Encryption failed: " + e.getMessage());
            e.printStackTrace();
            System.exit(1);
        }
    }

    public static void encrypt(String inputPath, String outputPath) throws Exception {
        Path input = Paths.get(inputPath);
        Path output = Paths.get(outputPath);

        byte[] plaintext = Files.readAllBytes(input);
        byte[] encrypted = encrypt(plaintext);
        Files.write(output, encrypted);
    }

    public static byte[] encrypt(byte[] plaintext) throws Exception {
        // Generate random IV
        byte[] iv = new byte[16];
        new SecureRandom().nextBytes(iv);

        // Initialize cipher
        Cipher cipher = Cipher.getInstance("AES/CBC/PKCS5Padding");
        SecretKeySpec keySpec = new SecretKeySpec(AES_KEY, "AES");
        IvParameterSpec ivSpec = new IvParameterSpec(iv);
        cipher.init(Cipher.ENCRYPT_MODE, keySpec, ivSpec);

        // Encrypt
        byte[] ciphertext = cipher.doFinal(plaintext);

        // Combine IV + ciphertext
        byte[] result = new byte[iv.length + ciphertext.length];
        System.arraycopy(iv, 0, result, 0, iv.length);
        System.arraycopy(ciphertext, 0, result, iv.length, ciphertext.length);

        return result;
    }

    public static byte[] decrypt(byte[] encrypted) throws Exception {
        if (encrypted.length < 17) {
            throw new IllegalArgumentException("Encrypted data too short");
        }

        // Extract IV (first 16 bytes)
        byte[] iv = new byte[16];
        System.arraycopy(encrypted, 0, iv, 0, 16);

        // Extract ciphertext
        byte[] ciphertext = new byte[encrypted.length - 16];
        System.arraycopy(encrypted, 16, ciphertext, 0, ciphertext.length);

        // Initialize cipher
        Cipher cipher = Cipher.getInstance("AES/CBC/PKCS5Padding");
        SecretKeySpec keySpec = new SecretKeySpec(AES_KEY, "AES");
        IvParameterSpec ivSpec = new IvParameterSpec(iv);
        cipher.init(Cipher.DECRYPT_MODE, keySpec, ivSpec);

        // Decrypt
        return cipher.doFinal(ciphertext);
    }
}
