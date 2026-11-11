#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "datalogger_service.hpp"
#include "mock_rtos_queue.hpp"
#include "mock_sensor.hpp"
#include "mock_storage.hpp"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgReferee;

namespace datalogger::testing {

class DataloggerServiceTest : public ::testing::Test {
protected:
    MockSensor mock_sensor_;
    MockStorage mock_storage_;
    FakeRtosQueue<hal::SensorReading> fake_queue_{4};
    DataloggerService service_{mock_sensor_, mock_storage_, fake_queue_, "/storage/log.csv", 3};
};

TEST_F(DataloggerServiceTest, InitializeSucceedsWhenSensorAndStorageAreReady) {
    EXPECT_CALL(mock_sensor_, init()).WillOnce(Return(true));
    EXPECT_CALL(mock_storage_, mount()).WillOnce(Return(true));

    EXPECT_TRUE(service_.initialize());
}

TEST_F(DataloggerServiceTest, InitializeFailsWhenSensorInitFails) {
    EXPECT_CALL(mock_sensor_, init()).WillOnce(Return(false));
    EXPECT_CALL(mock_storage_, mount()).Times(0);

    EXPECT_FALSE(service_.initialize());
}

TEST_F(DataloggerServiceTest, AcquisitionStepEnqueuesValidReadingToFreeRtosQueue) {
    const hal::SensorReading sample{1500, 26.75f, 58.5f, 3812.0f};
    EXPECT_CALL(mock_sensor_, read(_))
        .WillOnce(DoAll(SetArgReferee<0>(sample), Return(true)));

    EXPECT_TRUE(service_.acquisition_step(50));
    EXPECT_EQ(fake_queue_.messages_waiting(), 1u);
    EXPECT_EQ(service_.read_errors(), 0u);
}

TEST_F(DataloggerServiceTest, AcquisitionStepRejectsOutOfRangeTemperature) {
    const hal::SensorReading invalid_sample{1500, 250.0f, 50.0f, 3700.0f}; // > 125 C
    EXPECT_CALL(mock_sensor_, read(_))
        .WillOnce(DoAll(SetArgReferee<0>(invalid_sample), Return(true)));

    EXPECT_FALSE(service_.acquisition_step(50));
    EXPECT_EQ(fake_queue_.messages_waiting(), 0u);
    EXPECT_EQ(service_.read_errors(), 1u);
}

TEST_F(DataloggerServiceTest, AcquisitionStepCountsDroppedSamplesWhenQueueIsFull) {
    FakeRtosQueue<hal::SensorReading> tiny_queue(1);
    DataloggerService service(mock_sensor_, mock_storage_, tiny_queue, "/storage/log.csv", 2);

    const hal::SensorReading sample{1000, 25.0f, 50.0f, 3700.0f};
    EXPECT_CALL(mock_sensor_, read(_))
        .Times(2)
        .WillRepeatedly(DoAll(SetArgReferee<0>(sample), Return(true)));

    EXPECT_TRUE(service.acquisition_step(10));  // Preenche a fila de tamanho 1
    EXPECT_FALSE(service.acquisition_step(10)); // Fila cheia -> amostra descartada
    EXPECT_EQ(service.dropped_samples(), 1u);
}

TEST_F(DataloggerServiceTest, StorageStepFormatsCsvAndSyncsEveryNSamples) {
    // Enfileira 3 amostras simulando a Task Produtora do FreeRTOS
    fake_queue_.send({1000, 24.50f, 60.25f, 3750.0f}, 0);
    fake_queue_.send({2000, 24.75f, 60.50f, 3745.0f}, 0);
    fake_queue_.send({3000, 25.00f, 61.00f, 3740.0f}, 0);

    EXPECT_CALL(mock_storage_, append_line("/storage/log.csv", "1000,24.50,60.25,3750.0"))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_storage_, append_line("/storage/log.csv", "2000,24.75,60.50,3745.0"))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_storage_, append_line("/storage/log.csv", "3000,25.00,61.00,3740.0"))
        .WillOnce(Return(true));

    // O sync() deve ser chamado apenas na 3ª amostra (sync_every_n_samples = 3)
    EXPECT_CALL(mock_storage_, sync()).Times(1).WillOnce(Return(true));

    EXPECT_TRUE(service_.storage_step(100));
    EXPECT_TRUE(service_.storage_step(100));
    EXPECT_TRUE(service_.storage_step(100));
    EXPECT_EQ(service_.samples_logged(), 3u);
}

} // namespace datalogger::testing

