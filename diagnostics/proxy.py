import socket, threading, select, sys
def pipe(a,b):
    try:
        while True:
            r,_,_=select.select([a,b],[],[],60)
            if not r: break
            for s in r:
                d=s.recv(65536)
                if not d: return
                (b if s is a else a).sendall(d)
    except: pass
    finally:
        try:a.close()
        except:pass
        try:b.close()
        except:pass
def handle(c):
    try:
        c.settimeout(30)
        req=b""
        while b"\r\n\r\n" not in req:
            d=c.recv(4096)
            if not d: c.close();return
            req+=d
        line=req.split(b"\r\n")[0].decode("latin1")
        m,url,_=line.split(" ",2)
        if m=="CONNECT":
            host,port=url.split(":"); port=int(port)
            up=socket.create_connection((host,port),20)
            c.sendall(b"HTTP/1.1 200 Connection Established\r\n\r\n")
            pipe(c,up)
        else:
            from urllib.parse import urlparse
            u=urlparse(url); host=u.hostname; port=u.port or 80
            up=socket.create_connection((host,port),20)
            path=u.path or "/"
            if u.query: path+="?"+u.query
            newreq=req.replace(url.encode(),path.encode(),1)
            up.sendall(newreq)
            pipe(c,up)
    except Exception as e:
        try:c.close()
        except:pass
s=socket.socket(); s.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1)
s.bind(("0.0.0.0",8080)); s.listen(128)
print("proxy on 8080",flush=True)
while True:
    c,_=s.accept(); threading.Thread(target=handle,args=(c,),daemon=True).start()
