#include <iostream>
#include "application/ResourceVersionManager.h"
#include "Utility.h"

static void TestReaderPass()
{
    TUI::Application::ResourceVersionManager<std::string> manager{};
    manager.ReadBy({"test", "resource"}, "1");
    manager.CheckReaderVersion({"test", "resource"}, "2");
}

static void TestReaderFail()
{
    TUI::Application::ResourceVersionManager<std::string> manager{};
    manager.ReadBy({"test", "resource"}, "1");
    try
    {
        manager.CheckReaderVersion({"test", "resource"}, "1");
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
    TUI::Application::ResourceVersionManager<std::string> manager{};
    manager.WrittenBy({"test", "resource"}, "1");
    manager.CheckWriterVersion({"test", "resource"}, "1");
}

static void TestWriterFail()
{
    TUI::Application::ResourceVersionManager<std::string> manager{};
    manager.WrittenBy({"test", "resource"}, "1");
    try
    {
        manager.CheckWriterVersion({"test", "resource"}, "2");
        AssertWithMessage(false, "Expected CONFLICT exception, but none was thrown.");
    }
    catch (const TUI::Schema::Rpc::Exception& e)
    {
        AssertWithMessage(e.get_code() == TUI::Schema::Rpc::ErrorCode::CONFLICT,
            "Expected CONFLICT exception, got: " + std::to_string(e.get_code()));
    }
}

static void TestWriteAfterRead()
{
    TUI::Application::ResourceVersionManager<std::string> manager{};
    manager.ReadBy({"test", "resource"}, "1");
    manager.CheckWriterVersion({"test", "resource"}, "1");
}

static void TestWrittenByAnother()
{
    TUI::Application::ResourceVersionManager<std::string> manager{};
    manager.WrittenBy({"test", "resource"}, "1");
    manager.WrittenBy({"test", "resource"}, "2");
    try
    {
        manager.CheckWriterVersion({"test", "resource"}, "1");
        AssertWithMessage(false, "Expected CONFLICT exception, but none was thrown.");
    }
    catch (const TUI::Schema::Rpc::Exception& e)
    {
        AssertWithMessage(e.get_code() == TUI::Schema::Rpc::ErrorCode::CONFLICT,
            "Expected CONFLICT exception, got: " + std::to_string(e.get_code()));
    }
}

int main(int argc, char const *argv[])
{
    (void)argc;
    (void)argv;

    RunTest(TestReaderPass());
    RunTest(TestReaderFail());
    RunTest(TestWriterPass());
    RunTest(TestWriterFail());
    RunTest(TestWriteAfterRead());
    RunTest(TestWrittenByAnother());

    return 0;
}


