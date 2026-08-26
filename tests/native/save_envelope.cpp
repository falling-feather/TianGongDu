#include <tgd/contracts/save_envelope.hpp>
#include <tgd/contracts/sha256.hpp>
#include <tgd/contracts/tgd_web_abi.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

bool expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "save envelope failure: " << message << '\n';
    }
    return condition;
}

[[nodiscard]] std::string hex(std::span<const std::uint8_t> bytes) {
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (const auto byte : bytes) {
        stream << std::setw(2) << static_cast<unsigned>(byte);
    }
    return stream.str();
}

[[nodiscard]] tgd::contracts::SaveEnvelopeV1 fixture() {
    tgd::contracts::SaveEnvelopeV1 envelope;
    envelope.profile_id = {0x0011223344556677ULL, 0x8899aabbccddeeffULL};
    envelope.snapshot_id = {0x1021324354657687ULL, 0x98a9bacbdcedfe0fULL};
    envelope.parent_snapshot_id = {0x2031425364758697ULL, 0xa8b9cadbecfd0e1fULL};
    envelope.package_set_id = {0x30415263748596a7ULL, 0xb8c9daebfc0d1e2fULL};
    envelope.created_logical_sequence = 42;
    envelope.checkpoint_kind = tgd::contracts::CheckpointKind::chapter_milestone;
    constexpr std::string_view payload = "f1-profile-snapshot-v1";
    envelope.payload.assign(payload.begin(), payload.end());
    return envelope;
}

}  // namespace

int main() {
    using tgd::contracts::SaveEnvelopeError;

    static_assert(sizeof(tgd_web_abi_message_header) == TGD_WEB_ABI_MESSAGE_HEADER_BYTES);
    static_assert(TGD_WEB_ABI_MAX_MESSAGE_BYTES == 256U * 1024U);

    bool ok = true;
    constexpr std::array<std::uint8_t, 3> abc{'a', 'b', 'c'};
    ok &= expect(
        hex(tgd::contracts::sha256(abc)) ==
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        "SHA-256 matches the published abc vector"
    );
    ok &= expect(
        hex(tgd::contracts::sha256({})) ==
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
        "SHA-256 matches the published empty vector"
    );

    const auto source = fixture();
    const auto encoded = tgd::contracts::encode_save_envelope(source);
    ok &= expect(encoded.error == SaveEnvelopeError::none, "fixture encodes");
    if (encoded.error != SaveEnvelopeError::none) {
        return EXIT_FAILURE;
    }
    ok &= expect(
        encoded.bytes.size() ==
            tgd::contracts::save_envelope_v1_header_bytes + source.payload.size(),
        "v1 header size is fixed"
    );
    const auto decoded = tgd::contracts::decode_save_envelope(encoded.bytes);
    ok &= expect(decoded.error == SaveEnvelopeError::none, "fixture decodes");
    if (decoded.error != SaveEnvelopeError::none) {
        return EXIT_FAILURE;
    }
    ok &= expect(decoded.envelope.profile_id == source.profile_id, "profile id round-trips");
    ok &= expect(decoded.envelope.snapshot_id == source.snapshot_id, "snapshot id round-trips");
    ok &= expect(decoded.envelope.parent_snapshot_id == source.parent_snapshot_id, "parent id round-trips");
    ok &= expect(decoded.envelope.package_set_id == source.package_set_id, "package id round-trips");
    ok &= expect(decoded.envelope.payload == source.payload, "payload round-trips");
    const auto reencoded = tgd::contracts::encode_save_envelope(decoded.envelope);
    ok &= expect(
        reencoded.error == SaveEnvelopeError::none && reencoded.bytes == encoded.bytes,
        "canonical bytes round-trip"
    );

    auto invalid_magic = encoded.bytes;
    invalid_magic[0] ^= 0xffU;
    ok &= expect(
        tgd::contracts::decode_save_envelope(invalid_magic).error ==
            SaveEnvelopeError::invalid_magic,
        "invalid magic is rejected"
    );
    auto unsupported_version = encoded.bytes;
    unsupported_version[8] = 2;
    ok &= expect(
        tgd::contracts::decode_save_envelope(unsupported_version).error ==
            SaveEnvelopeError::unsupported_version,
        "unknown major is rejected"
    );
    ok &= expect(
        tgd::contracts::decode_save_envelope(
            std::span{encoded.bytes}.first(tgd::contracts::save_envelope_v1_header_bytes - 1)
        ).error == SaveEnvelopeError::truncated,
        "truncated header is rejected"
    );
    auto trailing_bytes = encoded.bytes;
    trailing_bytes.push_back(0);
    ok &= expect(
        tgd::contracts::decode_save_envelope(trailing_bytes).error ==
            SaveEnvelopeError::trailing_bytes,
        "trailing bytes are rejected"
    );
    auto corrupt_payload = encoded.bytes;
    corrupt_payload.back() ^= 0xffU;
    ok &= expect(
        tgd::contracts::decode_save_envelope(corrupt_payload).error ==
            SaveEnvelopeError::payload_hash_mismatch,
        "payload corruption is rejected"
    );
    auto corrupt_header = encoded.bytes;
    corrupt_header[96] ^= 0x01U;
    ok &= expect(
        tgd::contracts::decode_save_envelope(corrupt_header).error ==
            SaveEnvelopeError::envelope_hash_mismatch,
        "header corruption is rejected"
    );
    auto oversized = source;
    oversized.payload.resize(tgd::contracts::max_save_payload_bytes + 1);
    ok &= expect(
        tgd::contracts::encode_save_envelope(oversized).error ==
            SaveEnvelopeError::payload_too_large,
        "oversized payloads fail before encoding"
    );
    auto self_parent = source;
    self_parent.parent_snapshot_id = self_parent.snapshot_id;
    ok &= expect(
        tgd::contracts::encode_save_envelope(self_parent).error ==
            SaveEnvelopeError::invalid_header,
        "self-parent snapshots are rejected"
    );
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
