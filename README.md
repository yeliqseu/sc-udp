# Streaming Coded UDP

This repo contains the ns-3 implementation of streaming coded UDP, a UDP-based reliable packet transportation scheme powered by streaming coding. This work has been accepted and will be published soon in _China Communications_:

> Y. Li, J. Yu, L. Chen, Y. Hu, X. Chen, and J. Wang, "Packet Transport for Maritime Communications: A Streaming Coded UDP Approach," in _China Communications_, Sept. 2022.

The raw data from which the figures of the paper are plotted can be downloaded at:
```
https://1drv.ms/u/s!AufP1GR8lna4hMcwerGH4lwEfYZvkA?e=zX7fPh
```
The scripts for processing them are in the `datascripts` folder of this repo.

The streaming coded UDP implementation is under `ns-3.33-scudp/examples/streamc-udp/`, which is as an ``Application`` in ns-3.33. To run it,  first download and compile the streaming code library 
```
$ cd some_folder_outside_ns-3.33-scudp
$ git clone https://github.com/yeliqseu/streamc.git
$ cd streamc && make libstreamc.a
```

Change the following paths accordingly in `ns-3.33-scudp/examples/streamc-udp/wscript`, so that ns-3 can find and link to the streaming code library (i.e., libstreamc.a) to perform coding/decoding:
```
obj.env.append_value('CXXFLAGS','-I/path/to/the/folder/containing/libstreamc.a/')
obj.env.append_value('LINKFLAGS',['-L/path/to/the/folder/containing/libstreamc.a/'])
```
And then
```
$ ./waf configure --enable-examples --disable-werror --disable-python
$ ./waf build
```

Run the following command to see available parameters for testing the scheme:

```
$ ./waf configure --enable-examples --disable-werror --disable-python
$ ./waf build
$ ./waf --run "stream-server-client-sim --help"
```

For comparison, `ns-3.33-scudp/src/internet/model/` also contains BBR and Copa implementations (thanks to @SoonyangZhang for porting them from mvfst). Use the following command to see how to test them and compare with SC-UDP:

```
$ ./waf --run "mixed-flow-comparison --help"
```

The FEC-enhanced TCP of the following paper is also implemented, which can be turned on using `--tcpEnableFec` with `mixed-flow-comparison`. The implementation is via TCP option. See `ns-3.33-scudp/src/internet/model/tcp-option.{h,cc}, tcp-option-fec.{h,cc}, tcp-socket-base.{h,cc}` for details (a diff with vanilla ns-3.33 would be helpful).

> S. Ferlin, S. Kucera, H. Claussen and Ö. Alay, "MPTCP Meets FEC: Supporting Latency-Sensitive Applications Over Heterogeneous Networks," in IEEE/ACM Transactions on Networking, vol. 26, no. 5, pp. 2005-2018, Oct. 2018, doi: 10.1109/TNET.2018.2864192.

