#ifndef PixelFireworks_Error_hpp
#define PixelFireworks_Error_hpp
struct Error final {
    const char* Message;
    int Code = 1;
};
int Check(int result, const char* operation);
#endif

