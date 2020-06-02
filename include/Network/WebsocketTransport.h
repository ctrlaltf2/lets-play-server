/**
  * @file WebsocketTransport.h
  *
  * @author ctrlaltf2
  */
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#define _WEBSOCKETPP_CPP11_THREAD_

#include <websocketpp/common/connection_hdl.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>


class WebsocketTransport {

    using websocketpp::lib::placeholders::_1;
    using websocketpp::lib::placeholders::_2;

    websocketpp::server<websocketpp::config::asio> server_;

public:
    WebsocketTransport();

    void sendPlaintext(std::string data) override;

    void sendBinary(std::vector<std::uint8_t> data) override;

    /*
     * WebsocketTransport w;
     *
     * w.onConnect(LetsPlayServer::OnConnect);
     *
     * WebsocketTransport::onConnect(connection_hdl hdl) {
     *      // do websocketpp specific things
     *      ...
     *
     *      ConnectionInfo i = IP;
     *
     *      // Hand off execution to the callback
     *      cb_onConnect_(i);
     * }
     *
     * LetsPlayServer::OnConnect(ConnectionInfo i) {
     *      LetsPlayUser user(i.IP());
     * }
     *
     * struct ConnectionInfo {
     *      std::string? ip;
     *
     *      std::string user_agent;
     *
     * }
     */
};
