#include "node.h"

#include <array>
#include "cbuffermanager.h"
#include "cSecretbox_wrapper.h"
#include "linuxtun.h"
#include "cAsio_udp.h"
#include "cSendmmsg_udp.h"
#include <thread>

void node::run() {
	std::array<unsigned char, crypto_secretbox_KEYBYTES> crypto_key;
	crypto_key.fill(0b10101010);
	assert(m_tun != nullptr);
	assert(m_io_service != nullptr);
    cBufferManager bufferManager(100, 1500 * 20); // for possible weld data
	while (true) {
//        std::vector<unsigned char> buffer(1500 * 20); // for possible weld data
        cBuffer & buffer = bufferManager.get_free_buffer_or_wait();
		size_t tun_read_size = m_tun->read_from_tun(buffer.data(), buffer.size());
		if( m_thread_pool ) {
            m_thread_pool->addJob([=,&buffer]() mutable {
					size_t encypted_message_size =
						m_crypto->encrypt(
							  buffer.data(), tun_read_size,
							  crypto_key.data(), crypto_key.size(),
							  buffer.data(), buffer.size());
					std::lock_guard<std::mutex> lg(m_udp_mutex);
                    size_t udp_sended = m_udp->send(buffer.data(), encypted_message_size, m_dst_addr);
                    buffer.release();
				});
		} else {
			size_t encypted_message_size =
				m_crypto->encrypt(
					  buffer.data(), tun_read_size,
					  crypto_key.data(), crypto_key.size(),
					  buffer.data(), buffer.size());
            size_t udp_sended = m_udp->send(buffer.data(), encypted_message_size, m_dst_addr);
            buffer.release();
		}
    }
}

void node::run_async_tun(size_t number_of_tun_threads) {
	std::array<unsigned char, crypto_secretbox_KEYBYTES> crypto_key;
	crypto_key.fill(0b10101010);
	cBufferManager bufferManager(number_of_tun_threads, 1500 * 20); // for possible weld data, 1 buffer for 1 thread
	std::vector<std::thread> thread_vector;
	
	for (size_t i = 0; i < number_of_tun_threads; i++) {
		cBuffer & buffer = bufferManager.get_free_buffer_or_wait();
		m_tun_async->async_read_from_tun(buffer.data(), buffer.size(),
			[&buffer, this, &crypto_key](size_t tun_read_size) {
			size_t encypted_message_size =
				m_crypto->encrypt(
					  buffer.data(), tun_read_size,
					  crypto_key.data(), crypto_key.size(),
					  buffer.data(), buffer.size());
				std::lock_guard<std::mutex> lg(m_udp_mutex);
				size_t udp_sended = m_udp->send(buffer.data(), encypted_message_size, m_dst_addr);
			}
		);
		
	}
	
	for (size_t i = 0; i < number_of_tun_threads; i++) {
		thread_vector.emplace_back([this]{
			m_tun_async->run();
		});
	}
	for (auto & thread : thread_vector)
		thread.join();
}

void node::async_read_from_tun_handler(size_t tun_read_size)
{
	
}
