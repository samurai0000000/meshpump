/*
 * test/TestLedMatrix.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <CppUTest/TestHarness.h>
#include <string>
#include <thread>
#include <atomic>
#include <unistd.h>
#include "LedMatrix.hxx"
#include "LgpioShim.h"

TEST_GROUP(LedMatrixTest)
{
    void setup()
    {
        lgShimReset();
    }

    void teardown()
    {
        lgShimReset();
    }
};

TEST(LedMatrixTest, LifecycleInitAndTeardown)
{
    {
        LedMatrix lm;
        CHECK_EQUAL(1, lm.intensity());
        CHECK_EQUAL(25, lm.delay());
        for (unsigned int y = 0; y < 4; y++) {
            CHECK_EQUAL(y + 1, lm.slowdownFactor(y));
            CHECK_EQUAL(0, lm.ttl(y));
        }
        CHECK(lgShimGetSpiWriteCount() > 0);
    }
}

TEST(LedMatrixTest, GracefulDegradeOnSpiOpenFailure)
{
    lgShimSetFailSpiOpen(1);
    LedMatrix lm;
    lm.setIntensity(5);
    lm.repaint();
    CHECK_EQUAL(5, lm.intensity());
}

TEST(LedMatrixTest, TextAndTtlOperations)
{
    LedMatrix lm;
    lm.setText(0, "ABC", 10);
    CHECK_EQUAL(10, lm.ttl(0));
    lm.setText(3, "LONGTEXT12345678", 20);
    CHECK_EQUAL(20, lm.ttl(3));

    /* Boundary row tests */
    lm.setText(4, "OOB", 5);
    CHECK_EQUAL(0, lm.ttl(4));
    CHECK_EQUAL(0, lm.ttl(99));

    lm.clear();
    for (unsigned int y = 0; y < 4; y++) {
        CHECK_EQUAL(30, lm.ttl(y));
    }
}

TEST(LedMatrixTest, WelcomeTextLifecycle)
{
    LedMatrix lm;
    lm.setWelcomeText(1, "WELCOME_1", false);
    lm.setText(1, "TEMP", 15);
    CHECK_EQUAL(15, lm.ttl(1));

    lm.setWelcomeText();
    CHECK_EQUAL(0, lm.ttl(1));

    lm.setWelcomeText(2, "APPLIED", true);
    CHECK_EQUAL(0, lm.ttl(2));

    /* Boundary row tests */
    lm.setWelcomeText(4, "INVALID", true);
}

TEST(LedMatrixTest, DelayAndSlowdownFactorClamping)
{
    LedMatrix lm;
    lm.setDelay(50);
    CHECK_EQUAL(50, lm.delay());

    lm.setSlowdownFactor(0, 0);
    CHECK_EQUAL(1, lm.slowdownFactor(0));

    lm.setSlowdownFactor(1, 10);
    CHECK_EQUAL(10, lm.slowdownFactor(1));

    lm.setSlowdownFactor(5, 5);
    CHECK_EQUAL(1, lm.slowdownFactor(5));
}

TEST(LedMatrixTest, DrawAndShiftBoundaries)
{
    LedMatrix lm;
    uint8_t glyph[8] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};

    lm.draw(0, 0, glyph);
    lm.drawSL(0, 0, 2, false, glyph);
    lm.drawSR(0, 0, 2, false, glyph);

    /* Boundary condition checks */
    lm.draw(4, 0, glyph);
    lm.draw(0, 4, glyph);
    lm.draw(0, 0, NULL);
    lm.drawSL(0, 0, 8, true, glyph);
    lm.drawSR(0, 0, 9, false, glyph);
    lm.repaint();
}

TEST(LedMatrixTest, SignedCharHighBitFontBounds)
{
    LedMatrix lm;
    std::string highChars;
    for (int c = 128; c < 256; c++) {
        highChars.push_back(static_cast<char>(c));
    }

    lm.setText(0, highChars, 0);
    lm.setText(1, "\x80\xFF\xFE\xFD", 0);
    lm.setDelay(2);
    lm.start();
    usleep(30000);
    lm.stop();
    lm.join();
}

TEST(LedMatrixTest, DestructorWithoutExplicitJoin)
{
    {
        LedMatrix lm;
        lm.setDelay(2);
        lm.setText(0, "SCROLLING TEST TEXT 123456789", 0);
        lm.start();
        usleep(10000);
    }
}

TEST(LedMatrixTest, ConcurrentMutations)
{
    LedMatrix lm;
    lm.setDelay(2);
    lm.setText(0, "INITIAL LONG SCROLL TEXT TO EXERCISE SLICE AND POS", 0);
    lm.start();

    std::atomic<bool> done(false);
    std::thread t1([&lm, &done]() {
        for (int i = 0; !done.load() && i < 50; i++) {
            lm.setText(0, "THREAD1_TEXT", 10);
            lm.clear();
            usleep(500);
        }
    });

    std::thread t2([&lm, &done]() {
        for (int i = 0; !done.load() && i < 50; i++) {
            lm.setWelcomeText(1, "WELCOME_T2", true);
            lm.setSlowdownFactor(1, (i % 5) + 1);
            usleep(500);
        }
    });

    std::thread t3([&lm, &done]() {
        for (int i = 0; !done.load() && i < 50; i++) {
            lm.setDelay((i % 10) + 2);
            lm.setIntensity((i % 15) + 1);
            lm.repaint();
            usleep(500);
        }
    });

    usleep(30000);
    done.store(true);
    t1.join();
    t2.join();
    t3.join();
    lm.stop();
    lm.join();
}

TEST(LedMatrixTest, TtlExpirationRestoresWelcome)
{
    LedMatrix lm;
    lm.setWelcomeText(0, "HOME", false);
    lm.setText(0, "TEMP", 1);
    CHECK_EQUAL(1, lm.ttl(0));
    lm.setDelay(5);
    lm.start();
    sleep(2);
    lm.stop();
    lm.join();
    CHECK_EQUAL(0, lm.ttl(0));
}

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
