#pragma once
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace ng::io {

    // Darf der Typ als rohe Bytes gespeichert werden?
    // Standard: nur wenn trivially copyable. Eigene Typen werden in
    // BspSceneSerialize.h per Spezialisierung freigeschaltet.
    template <class V>
    struct IsRawSerializable : std::bool_constant<std::is_trivially_copyable_v<V>> {};

    constexpr uint32_t FourCC(char a, char b, char c, char d) {
        return uint32_t(uint8_t(a)) | (uint32_t(uint8_t(b)) << 8) |
            (uint32_t(uint8_t(c)) << 16) | (uint32_t(uint8_t(d)) << 24);
    }

    constexpr uint32_t kFileMagic = FourCC('N', 'G', 'M', 'P');
    constexpr uint32_t kFileVersion = 1;

    // ---------------------------------------------------------------- Writer
    class ChunkWriter {
    public:
        explicit ChunkWriter(std::ostream& os) : m_os(os) {}

        void BeginChunk(uint32_t id, uint32_t version) {
            Pod(id);
            Pod(version);
            Pod<uint64_t>(0);                       // Platzhalter fuer Groesse
            m_stack.push_back(m_os.tellp());
        }
        void EndChunk() {
            std::streampos start = m_stack.back(); m_stack.pop_back();
            std::streampos end = m_os.tellp();
            uint64_t size = uint64_t(end - start);
            m_os.seekp(start - std::streamoff(sizeof(uint64_t)));
            Pod(size);
            m_os.seekp(end);
        }

        template <class V> void Pod(const V& v) {
            static_assert(IsRawSerializable<V>::value, "Typ nicht roh serialisierbar");
            m_os.write(reinterpret_cast<const char*>(&v), sizeof(V));
        }
        void Bytes(const void* p, size_t n) {
            m_os.write(static_cast<const char*>(p), std::streamsize(n));
        }

        void String(const std::string& s) {
            Pod<uint32_t>(uint32_t(s.size()));
            Bytes(s.data(), s.size());
        }
        template <class V> void PodVector(const std::vector<V>& v) {
            static_assert(IsRawSerializable<V>::value, "Typ nicht roh serialisierbar");
            Pod<uint64_t>(uint64_t(v.size()));
            if (!v.empty()) Bytes(v.data(), v.size() * sizeof(V));
        }
        bool Good() const { return m_os.good(); }

    private:
        std::ostream& m_os;
        std::vector<std::streampos> m_stack;
    };

    // ---------------------------------------------------------------- Reader
    class ChunkReader {
    public:
        ChunkReader(const uint8_t* data, size_t size, uint32_t version)
            : m_p(data), m_end(data + size), m_version(version) {
        }

        uint32_t Version() const { return m_version; }
        size_t Remaining() const { return size_t(m_end - m_p); }

        template <class V> V Pod() {
            static_assert(IsRawSerializable<V>::value, "Typ nicht roh serialisierbar");
            V v;
            Bytes(&v, sizeof(V));
            return v;
        }
        void Bytes(void* dst, size_t n) {
            if (Remaining() < n) throw std::runtime_error("Chunk: unexpected end of data");
            std::memcpy(dst, m_p, n);
            m_p += n;
        }

        // Liest eine Anzahl und prueft, dass die Daten dafuer noch reichen koennen.
        uint64_t Count(size_t minBytesPerItem = 1) {
            uint64_t n = Pod<uint64_t>();
            if (minBytesPerItem == 0) minBytesPerItem = 1;
            if (n > Remaining() / minBytesPerItem)
                throw std::runtime_error("Chunk: invalid element count");
            return n;
        }

        std::string String() {
            uint32_t n = Pod<uint32_t>();
            if (n > Remaining()) throw std::runtime_error("Chunk: invalid string size");
            std::string s;
            s.resize(n);
            if (n) Bytes(s.data(), n);
            return s;
        }
        template <class V> std::vector<V> PodVector() {
            static_assert(IsRawSerializable<V>::value, "Typ nicht roh serialisierbar");
            uint64_t n = Count(sizeof(V));
            std::vector<V> v;
            v.resize(size_t(n));
            if (n) Bytes(v.data(), size_t(n) * sizeof(V));
            return v;
        }

    private:
        const uint8_t* m_p;
        const uint8_t* m_end;
        uint32_t m_version;
    };

} // namespace ng::io