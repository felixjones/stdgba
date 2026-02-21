#include <gba/dma>

#include <mgba_test.hpp>

// Test data
alignas(4) static unsigned int src_buffer[64];
alignas(4) static unsigned int dst_buffer[64];

// Fill values need to be in RAM for DMA to read them
alignas(4) static unsigned int fill_value_32 = 0xCAFEBABE;

int main() {
    // Initialize test data
    for (std::size_t i = 0; i < 64; ++i) {
        src_buffer[i] = i * 0x01010101;
        dst_buffer[i] = 0;
    }

    // dma::copy creates correct configuration
    {
        auto config = gba::dma::copy(src_buffer, dst_buffer, 64);

        ASSERT_TRUE(config.source == src_buffer);
        ASSERT_TRUE(config.destination == dst_buffer);
        ASSERT_EQ(config.units, 64);
        ASSERT_TRUE(config.control.enable);
        ASSERT_EQ(config.control.dma_type, gba::dma_type::word);
    }

    // dma::copy transfer works
    {
        // Reset destination
        for (auto& x : dst_buffer) x = 0;

        gba::reg_dma[3] = gba::dma::copy(src_buffer, dst_buffer, 64);

        // Wait for completion
        while (static_cast<gba::dma_control>(gba::reg_dmacnt_h[3]).enable) {}

        // Verify copy
        for (std::size_t i = 0; i < 64; ++i) {
            ASSERT_EQ(dst_buffer[i], src_buffer[i]);
        }
    }

    // dma::fill creates correct configuration
    {
        auto config = gba::dma::fill(&fill_value_32, dst_buffer, 64);

        ASSERT_TRUE(config.source == &fill_value_32);
        ASSERT_TRUE(config.destination == dst_buffer);
        ASSERT_EQ(config.units, 64);
        ASSERT_TRUE(config.control.enable);
        ASSERT_EQ(config.control.src_op, gba::src_op_fixed);
    }

    // dma::fill transfer works
    {
        // Reset destination
        for (auto& x : dst_buffer) x = 0;

        gba::reg_dma[3] = gba::dma::fill(&fill_value_32, dst_buffer, 64);

        // Wait for completion
        while (static_cast<gba::dma_control>(gba::reg_dmacnt_h[3]).enable) {}

        // Verify fill
        for (std::size_t i = 0; i < 64; ++i) {
            ASSERT_EQ(dst_buffer[i], fill_value_32);
        }
    }

    // dma::on_vblank creates repeating configuration
    {
        auto config = gba::dma::on_vblank(src_buffer, dst_buffer, 64);

        ASSERT_TRUE(config.control.enable);
        ASSERT_TRUE(config.control.repeat);
        ASSERT_EQ(config.control.dma_cond, gba::dma_cond_vblank);
        ASSERT_EQ(config.control.dest_op, gba::dest_op_increment_reload);
    }

    // dma::on_hblank creates HDMA configuration
    {
        auto config = gba::dma::on_hblank(src_buffer, dst_buffer, 1);

        ASSERT_TRUE(config.control.enable);
        ASSERT_TRUE(config.control.repeat);
        ASSERT_EQ(config.control.dma_cond, gba::dma_cond_hblank);
        ASSERT_EQ(config.control.dma_type, gba::dma_type::half);
    }

    // dma::to_fifo_a creates audio streaming configuration
    {
        auto config = gba::dma::to_fifo_a(src_buffer);

        ASSERT_TRUE(config.destination == reinterpret_cast<void*>(0x40000A0));
        ASSERT_EQ(config.units, 1);
        ASSERT_TRUE(config.control.repeat);
        ASSERT_EQ(config.control.dma_cond, gba::dma_cond_sound_fifo);
        ASSERT_EQ(config.control.dest_op, gba::dest_op_fixed);
    }

    // dma::to_fifo_b creates audio streaming configuration
    {
        auto config = gba::dma::to_fifo_b(src_buffer);

        ASSERT_TRUE(config.destination == reinterpret_cast<void*>(0x40000A4));
    }

    // Stopping DMA channel
    {
        // Start a repeating DMA
        gba::reg_dma[3] = gba::dma::on_vblank(src_buffer, dst_buffer, 64);
        ASSERT_TRUE(static_cast<gba::dma_control>(gba::reg_dmacnt_h[3]).enable);

        // Stop it
        gba::reg_dmacnt_h[3] = {};
        ASSERT_FALSE(static_cast<gba::dma_control>(gba::reg_dmacnt_h[3]).enable);
    }
}
