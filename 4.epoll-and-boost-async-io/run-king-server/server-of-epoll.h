#ifndef SERVEROFEPOLL_H
#define SERVEROFEPOLL_H

#include <iostream>
#include <vector>
#include <list>
#include <map>
using namespace std;

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/epoll.h>
#include <netinet/in.h>

namespace of_epoll {
    static struct msg {
        int flag;
        char id[24];
        unsigned int steps;
    } res;

    static map<string,unsigned int> status;
    static map<int,pair<msg,unsigned int>> requests;

    bool task(int fd) {
        auto& kv = requests[fd];
        auto& req = kv.first;
        auto& offset = kv.second;

        auto len = recv(fd,reinterpret_cast<char*>(&req) + offset,sizeof(req) - offset,MSG_NOSIGNAL);
        if(len <= 0) goto RET;

        offset += len;
        if(offset == sizeof(req)) offset = 0;
        else return false;

        switch(req.flag) {
        case 0:{
            cout << "run:" << req.steps << endl;
            req.steps++;
            status[req.id] = req.steps;
            if(-1 == send(fd,&req,sizeof(req),MSG_NOSIGNAL)) goto RET;
            break;}
        default:{
            auto runner_num = status.size();
            cout << "peek:" << runner_num << endl;

            if(-1 == send(fd,&runner_num,4,0)) goto RET;
            for(auto& kv : status) {
                fill_n(res.id,24,0);
                kv.first.copy(res.id,kv.first.size(),0);
                res.steps = kv.second;
                if(-1 == send(fd,&res,sizeof(res),MSG_NOSIGNAL)) goto RET;
                cout << res.id << ' ' << res.steps << endl;
            }

            return false;}
        }

        RET:
        requests.erase(fd);
        return true;
    }

    void server(unsigned short port) {
        vector<epoll_event> fds;

        int fd_self = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        int fds_num = 1;

        auto epfd = epoll_create1(0);

        epoll_event event;
        event.data.fd = fd_self;
        event.events = EPOLLIN;
        epoll_ctl(epfd,EPOLL_CTL_ADD,fd_self,&event);

        int tmp = 1;
        setsockopt(fd_self, SOL_SOCKET, SO_REUSEADDR, &tmp, sizeof(int));

        sockaddr_in addr_s,addr_c;
        socklen_t addr_c_len = sizeof(sockaddr_in);
        addr_s.sin_family = AF_INET;
        addr_s.sin_port = htons(port);
        addr_s.sin_addr.s_addr = INADDR_ANY;

        if(-1 == bind(fd_self, reinterpret_cast<sockaddr*>(&addr_s), sizeof(sockaddr))) goto RET;
        if(-1 == listen(fd_self, 1024)) goto RET;

        while(true) {
            fds.resize(fds_num);
            auto num = epoll_wait(epfd,fds.data(),fds_num,-1);
            if(-1 == num) goto RET;

            for(auto it=fds.begin();num > 0;it++,num--) {
                if(it->data.fd == fd_self) {
                    auto fd_new = accept(fd_self, reinterpret_cast<sockaddr*>(&addr_c), &addr_c_len);
                    if(fd_new == -1) {
                        cout << "accept:" << errno << endl;
                        goto RET;
                    }
                    cout << "new:" << fd_new << endl;
                    fds_num++;

                    event.data.fd = fd_new;
                    event.events = EPOLLIN;
                    epoll_ctl(epfd,EPOLL_CTL_ADD,fd_new,&event);
                    continue;
                }

                if(!task(it->data.fd)) continue;
                close(it->data.fd);

                event.data.fd = it->data.fd;
                event.events = EPOLLIN;
                epoll_ctl(epfd,EPOLL_CTL_DEL,it->data.fd,&event);

                cout << "close:" << it->data.fd << endl;
                fds_num--;
            }
        }

        RET:
        close(fd_self);
    }
}

#endif // SERVEROFEPOLL_H
