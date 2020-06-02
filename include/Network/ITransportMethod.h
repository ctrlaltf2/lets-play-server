/**
  * @file ITransportMethod.h
  *
  * @author ctrlaltf2
  */
#pragma once
#include <cstdint>
#include <string>
#include <vector>

class ITransportMethod {
public:
    virtual void sendPlaintext(std::string data) = 0;

    virtual void sendBinary(std::vector<std::uint8_t> data) = 0;
};
