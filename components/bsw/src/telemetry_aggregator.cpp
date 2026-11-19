#include "telemetry_aggregator.hpp"

#include <cstddef>

namespace datalogger::bsw {

TelemetryAggregator::TelemetryAggregator(const RtcService& rtc_service,
                                         const CanDecoderService& can_decoder,
                                         const AdcService& adc_service,
                                         const GpsService& gps_service)
    : rtc_service_(rtc_service),
      can_decoder_(can_decoder),
      adc_service_(adc_service),
      gps_service_(gps_service) {}

TelemetrySnapshot TelemetryAggregator::capture_snapshot(uint16_t lap_beacon,
                                                        uint64_t can_timeout_ms) {
    TelemetrySnapshot snap{};

    const RtcTimestamp ts = rtc_service_.sample_timestamp();
    snap.monotonic_ms = ts.monotonic_ms;
    snap.epoch_ms = ts.epoch_ms;
    snap.rtc_synchronized = ts.synchronized;

    ++sample_seq_;
    snap.sample_seq = sample_seq_;
    snap.lap_beacon = lap_beacon;

    uint16_t adc_mask = 0u;
    for (std::size_t i = 0u; i < hal::kAdcChannelCount; ++i) {
        const auto ch = static_cast<hal::AdcChannelId>(i);
        const auto& reading = adc_service_.channel_reading(ch);
        snap.adc_physical[i] = reading.physical_value;
        if (reading.valid) {
            adc_mask = static_cast<uint16_t>(adc_mask | (1u << i));
        }
    }
    snap.adc_valid_mask = adc_mask;

    const auto& can_state = can_decoder_.state();
    snap.front_controls = can_state.front_controls;
    snap.wheels = can_state.wheels;
    snap.imu = can_state.imu;
    snap.ecu = can_state.ecu;

    uint8_t can_mask = 0u;
    for (std::size_t i = 0u; i < VehicleCanState::kNodeCount; ++i) {
        const auto node = static_cast<CanNodeId>(i);
        if (can_decoder_.is_node_online(node, ts.monotonic_ms, can_timeout_ms)) {
            can_mask = static_cast<uint8_t>(can_mask | (1u << i));
        }
    }
    snap.can_node_online_mask = can_mask;

    const auto& gps = gps_service_.latest_data();
    snap.gps_latitude_deg = gps.latitude_deg;
    snap.gps_longitude_deg = gps.longitude_deg;
    snap.gps_altitude_m = gps.altitude_m;
    snap.gps_speed_kmh = gps.speed_kmh;
    snap.gps_speed_mps = gps.speed_mps;
    snap.gps_course_deg = gps.course_deg;
    snap.gps_satellites = gps.satellites;
    snap.gps_has_fix = gps.has_fix;

    return snap;
}

bool TelemetryAggregator::publish_snapshot(osal::IOsalQueue<TelemetrySnapshot>& target_queue,
                                           uint32_t queue_timeout_ms,
                                           uint16_t lap_beacon,
                                           uint64_t can_timeout_ms) {
    const TelemetrySnapshot snap = capture_snapshot(lap_beacon, can_timeout_ms);
    if (!target_queue.send(snap, queue_timeout_ms)) {
        ++dropped_count_;
        return false;
    }
    ++published_count_;
    return true;
}

void TelemetryAggregator::reset() {
    sample_seq_ = 0u;
    published_count_ = 0u;
    dropped_count_ = 0u;
}

} // namespace datalogger::bsw
