#include <gba/dma>
#include <gba/testing>

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

        gba::test.is_true(config.source == src_buffer);
        gba::test.is_true(config.destination == dst_buffer);
        gba::test.eq(config.units, 64);
        gba::test.is_true(config.control.enable);
        gba::test.eq(config.control.dma_type, gba::dma_type::word);
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
            gba::test.eq(dst_buffer[i], src_buffer[i]);
        }
    }

    // dma::fill creates correct configuration
    {
        auto config = gba::dma::fill(&fill_value_32, dst_buffer, 64);

        gba::test.is_true(config.source == &fill_value_32);
        gba::test.is_true(config.destination == dst_buffer);
        gba::test.eq(config.units, 64);
        gba::test.is_true(config.control.enable);
        gba::test.eq(config.control.src_op, gba::src_op_fixed);
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
            gba::test.eq(dst_buffer[i], fill_value_32);
        }
    }

    // dma::on_vblank creates repeating configuration
    {
        auto config = gba::dma::on_vblank(src_buffer, dst_buffer, 64);

        gba::test.is_true(config.control.enable);
        gba::test.is_true(config.control.repeat);
        gba::test.eq(config.control.dma_cond, gba::dma_cond_vblank);
        gba::test.eq(config.control.dest_op, gba::dest_op_increment_reload);
    }

    // dma::on_hblank creates HDMA configuration
    {
        auto config = gba::dma::on_hblank(src_buffer, dst_buffer, 1);

        gba::test.is_true(config.control.enable);
        gba::test.is_true(config.control.repeat);
        gba::test.eq(config.control.dma_cond, gba::dma_cond_hblank);
        gba::test.eq(config.control.dma_type, gba::dma_type::half);
    }

    // dma::to_fifo_a creates audio streaming configuration
    {
        auto config = gba::dma::to_fifo_a(src_buffer);

        gba::test.is_true(config.destination == reinterpret_cast<void*>(0x40000A0));
        gba::test.eq(config.units, 1);
        gba::test.is_true(config.control.repeat);
        gba::test.eq(config.control.dma_cond, gba::dma_cond_sound_fifo);
        gba::test.eq(config.control.dest_op, gba::dest_op_fixed);
    }

    // dma::to_fifo_b creates audio streaming configuration
    {
        auto config = gba::dma::to_fifo_b(src_buffer);

        gba::test.is_true(config.destination == reinterpret_cast<void*>(0x40000A4));
    }

    // Stopping DMA channel
    {
        // Start a repeating DMA
        gba::reg_dma[3] = gba::dma::on_vblank(src_buffer, dst_buffer, 64);
        gba::test.is_true(static_cast<gba::dma_control>(gba::reg_dmacnt_h[3]).enable);

        // Stop it
        gba::reg_dmacnt_h[3] = {};
        gba::test.is_false(static_cast<gba::dma_control>(gba::reg_dmacnt_h[3]).enable);
    }
    return gba::test.finish();
}
