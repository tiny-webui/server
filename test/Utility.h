#pragma once

#define AssertWithMessage(expr, msg) \
    if (!(expr)) \
    { \
        std::cerr << "Assertion failed: " << msg << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
        abort(); \
    }

#define RunAsyncTest(asyncExpr) \
    do \
    { \
        try \
        { \
            std::cout << "Running test: " #asyncExpr << std::endl; \
            co_await (asyncExpr); \
            std::cout << "Test " #asyncExpr " completed successfully." << std::endl; \
        } \
        catch(const std::exception& e) \
        { \
            AssertWithMessage(false, "Unhandled exception: " + std::string(e.what())); \
        } \
        catch(...) \
        { \
            AssertWithMessage(false, "Unhandled unknown exception"); \
        } \
    } while (0)

#define RunTest(expr) \
    do \
    { \
        try \
        { \
            std::cout << "Running test: " #expr << std::endl; \
            (expr); \
            std::cout << "Test " #expr " completed successfully." << std::endl; \
        } \
        catch(const std::exception& e) \
        { \
            AssertWithMessage(false, "Unhandled exception: " + std::string(e.what())); \
        } \
        catch(...) \
        { \
            AssertWithMessage(false, "Unhandled unknown exception"); \
        } \
    } while (0)
