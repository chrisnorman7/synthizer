#include <memory>

#include "synthizer/decoding.hpp"
#include "synthizer/byte_stream.hpp"

namespace synthizer {

std::shared_ptr<AudioDecoder> decodeOgg(std::shared_ptr<LookaheadByteStream> stream) {
	(void)stream;

	return nullptr;
}


}
