#include <iostream>
#include "application/ResourceVersionManager.h"
#include "Utility.h"

static void TestReaderPass()
{
    auto manager =  TUI::Application::ResourceVersionManager<std::string>::Create();
    {
        /** 1 is not up to date. Thus, the read should be valid. */
        auto lock = manager->GetReadLock({"test", "resource"}, "1");
    }
    /** 2 is not up to date. Thus, the read should be valid. */
    auto lock = manager->GetReadLock({"test", "resource"}, "2");
}

static void TestReaderFail()
{
    auto manager = TUI::Application::ResourceVersionManager<std::string>::Create();
    {
        auto lock = manager->GetReadLock({"test", "resource"}, "1");
    }
    try
    {
        /** 1 is up to date. The read should not be valid. */
        auto lock = manager->GetReadLock({"test", "resource"}, "1");
        AssertWithMessage(false, "Expected NOT_MODIFIED exception, but none was thrown.");
    }
    catch (const TUI::Schema::Rpc::Exception& e)
    {
        AssertWithMessage(e.get_code() == TUI::Schema::Rpc::ErrorCode::NOT_MODIFIED,
            "Expected NOT_MODIFIED exception, got: " + std::to_string(e.get_code()));   
    }
}

static void TestWriterPass()
{
    auto manager = TUI::Application::ResourceVersionManager<std::string>::Create();
    {
        auto lock = manager->GetReadLock({"test", "resource"}, "1");
    }
    auto lock = manager->GetWriteLock({"test", "resource"}, "1");
}

static void TestWriterFail()
{
    auto manager = TUI::Application::ResourceVersionManager<std::string>::Create();
    {
        auto lock = manager->GetReadLock({"test", "resource"}, "1");
    }
    {
        auto lock = manager->GetReadLock({"test", "resource"}, "2");
    }
    {
        /** Now 1 is the only one up to date. */
        auto lock = manager->GetWriteLock({"test", "resource"}, "1");
    }
    try
    {
        /** 2 is not up to date. It should not write. */
        auto lock = manager->GetWriteLock({"test", "resource"}, "2");
        AssertWithMessage(false, "Expected CONFLICT exception, but none was thrown.");
    }
    catch (const TUI::Schema::Rpc::Exception& e)
    {
        AssertWithMessage(e.get_code() == TUI::Schema::Rpc::ErrorCode::CONFLICT,
            "Expected CONFLICT exception, got: " + std::to_string(e.get_code()));
    }
}

static void TestReadWhileReading()
{
    auto manager = TUI::Application::ResourceVersionManager<std::string>::Create();
    auto lock1 = manager->GetReadLock({"test", "resource"}, "1");
    auto lock2 = manager->GetReadLock({"test", "resource"}, "2");
}

static void TestWriteWhileReading()
{
    auto manager = TUI::Application::ResourceVersionManager<std::string>::Create();
    {
        /** Make 2 up to date so it can write. */
        auto lock = manager->GetReadLock({"test", "resource"}, "2");
    }
    auto lock1 = manager->GetReadLock({"test", "resource"}, "1");
    try
    {
        auto lock2 = manager->GetWriteLock({"test", "resource"}, "2");
        AssertWithMessage(false, "Expected LOCKED exception, but none was thrown.");
    }
    catch (const TUI::Schema::Rpc::Exception& e)
    {
        AssertWithMessage(e.get_code() == TUI::Schema::Rpc::ErrorCode::LOCKED,
            "Expected LOCKED exception, got: " + std::to_string(e.get_code()));
    }
}

static void TestReadWhileWriting()
{
    auto manager = TUI::Application::ResourceVersionManager<std::string>::Create();
    {
        /** Make 1 up to date so it can write. */
        auto lock = manager->GetReadLock({"test", "resource"}, "1");
    }
    auto lock1 = manager->GetWriteLock({"test", "resource"}, "1");
    try
    {
        auto lock2 = manager->GetReadLock({"test", "resource"}, "2");
        AssertWithMessage(false, "Expected LOCKED exception, but none was thrown.");
    }
    catch (const TUI::Schema::Rpc::Exception& e)
    {
        AssertWithMessage(e.get_code() == TUI::Schema::Rpc::ErrorCode::LOCKED,
            "Expected LOCKED exception, got: " + std::to_string(e.get_code()));
    }
}

static void TestWriteWhileWriting()
{
    auto manager = TUI::Application::ResourceVersionManager<std::string>::Create();
    {
        auto lock = manager->GetReadLock({"test", "resource"}, "1");
    }
    auto lock1 = manager->GetWriteLock({"test", "resource"}, "1");
    try
    {
        auto lock2 = manager->GetWriteLock({"test", "resource"}, "2");
        AssertWithMessage(false, "Expected LOCKED exception, but none was thrown.");
    }
    catch (const TUI::Schema::Rpc::Exception& e)
    {
        AssertWithMessage(e.get_code() == TUI::Schema::Rpc::ErrorCode::LOCKED,
            "Expected LOCKED exception, got: " + std::to_string(e.get_code()));
    }
}

static void TestDoNotConfirm()
{
    auto manager = TUI::Application::ResourceVersionManager<std::string>::Create();
    {
        auto lock = manager->GetReadLock({"test", "resource"}, "1");
        lock.DoNotConfirm();
    }
    /** This should not throw, as the lock was not confirmed. */
    auto lock = manager->GetReadLock({"test", "resource"}, "1");
}

static void TestNoConfirmationOnException()
{
    auto manager = TUI::Application::ResourceVersionManager<std::string>::Create();
    try
    {
        auto lock = manager->GetReadLock({"test", "resource"}, "1");
        throw std::runtime_error("Test exception");
    }
    catch(...)
    {
    }
    /** This should not throw, as the lock was not confirmed. */
    auto lock = manager->GetReadLock({"test", "resource"}, "1");
}

static void TestDelete()
{
    auto manager = TUI::Application::ResourceVersionManager<std::string>::Create();
    {
        auto lock = manager->GetReadLock({"test", "resource"}, "1");
    }
    {
        auto lock = manager->GetDeleteLock({"test", "resource"}, "1");
    }
    /** 1 should no longer be up to date with the resource */
    auto lock = manager->GetReadLock({"test", "resource"}, "1");
}

int main(int argc, char const *argv[])
{
    (void)argc;
    (void)argv;

    RunTest(TestReaderPass());
    RunTest(TestReaderFail());
    RunTest(TestWriterPass());
    RunTest(TestWriterFail());
    RunTest(TestReadWhileReading());
    RunTest(TestWriteWhileReading());
    RunTest(TestReadWhileWriting());
    RunTest(TestWriteWhileWriting());
    RunTest(TestDoNotConfirm());
    RunTest(TestNoConfirmationOnException());
    RunTest(TestDelete());

    return 0;
}


