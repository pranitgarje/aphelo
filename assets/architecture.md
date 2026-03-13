sequenceDiagram
    participant Client
    participant ServerLoop as Server (Event Loop)
    participant ReqHandler as Request Handler
    participant HMap as Hash Table (HMap)

    Client->>ServerLoop: connect(127.0.0.1:12345)
    ServerLoop->>ServerLoop: Accept connection (handle_accept)
    ServerLoop->>ServerLoop: Set non-blocking (O_NONBLOCK)
    Client->>ServerLoop: write_all([Total Len, nstr, len1, str1...])
    ServerLoop->>ServerLoop: poll() detects POLLIN
    ServerLoop->>ReqHandler: handle_read(conn)
    ReqHandler->>ReqHandler: read() into temporary buffer
    ReqHandler->>ReqHandler: buf_append(conn->incoming)

    rect rgb(30, 30, 30)
        Note over ReqHandler: try_one_request() loop
        ReqHandler->>ReqHandler: parse_req() validates length & formats
        
        Note over ReqHandler: THE PLACEHOLDER TRICK
        ReqHandler->>ReqHandler: out.insert(4 bytes of zeroes)
        ReqHandler->>ReqHandler: do_request(cmd, conn->outgoing)

        alt cmd == "set"
            ReqHandler->>HMap: hm_lookup / hm_insert
            HMap-->>ReqHandler: success
            ReqHandler->>ReqHandler: out_nil(out) appends [TAG_NIL]
        else cmd == "get"
            ReqHandler->>HMap: hm_lookup(dummy_key)
            HMap-->>ReqHandler: existing HNode or NULL
            ReqHandler->>ReqHandler: out_str(out) OR out_nil(out)
        else cmd == "del"
            ReqHandler->>HMap: hm_delete(dummy_key)
            HMap-->>ReqHandler: detached HNode or NULL
            ReqHandler->>ReqHandler: out_int(out) appends [TAG_INT] + value
        end

        ReqHandler->>ReqHandler: memcpy() overwrites placeholder with real length
        ReqHandler->>ReqHandler: buf_consume(conn->incoming)
    end

    ServerLoop->>ServerLoop: poll() detects POLLOUT
    ServerLoop->>Client: write(conn->outgoing)
    ReqHandler->>ReqHandler: buf_consume(conn->outgoing)
    
    Client->>Client: read_full() parses header and body
    Note over Client: TLV RECURSIVE PARSING
    Client->>Client: print_response(data, size)
    Client->>Client: Prints: "(str) val", "(nil)", or "(int) 1"