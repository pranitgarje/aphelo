#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <string>
#include <vector>

static void msg(const char *msg) {
    fprintf(stderr, "%s\n", msg);
}
static void die(const char *msg) {
    int err = errno;
    fprintf(stderr, "[%d] %s\n", err, msg);
    abort();
}
static int32_t read_full(int fd, char *buf, size_t n) {
    while (n > 0) {
        ssize_t rv = read(fd, buf, n);
        if (rv <= 0) {
            return -1;  // error, or unexpected EOF
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}
static int32_t write_all(int fd, const char *buf, size_t n) {
    while (n > 0) {
        ssize_t rv = write(fd, buf, n);
        if (rv <= 0) {
            return -1;  // error
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}
 const size_t k_max_msg = 4096;  
static int32_t send_req(int fd, const std::vector<std::string> &cmd) {
    uint32_t len = 4;
    for (const std::string &s : cmd) {
        len += 4 + s.size();
    }
    if (len > k_max_msg) {
        return -1;
    }

    char wbuf[4 + k_max_msg];
    memcpy(&wbuf[0], &len, 4);  // assume little endian
    uint32_t n = cmd.size();
    memcpy(&wbuf[4], &n, 4);
    size_t cur = 8;
    for (const std::string &s : cmd) {
        uint32_t p = (uint32_t)s.size();
        memcpy(&wbuf[cur], &p, 4);
        memcpy(&wbuf[cur + 4], s.data(), s.size());
        cur += 4 + s.size();
    }
    return write_all(fd, wbuf, 4 + len);
}

enum {
    TAG_NIL = 0,    // nil
    TAG_ERR = 1,    // error code + msg
    TAG_STR = 2,    // string
    TAG_INT = 3,    // int64
    TAG_DBL = 4,    // double
    TAG_ARR = 5,    // array
};

static int32_t print_response(const uint8_t *data,size_t size){
    if(size<1){
        msg("bad response");
        return -1;
    }
    switch(data[0]){
        /*If the tag is TAG_NIL (usually meaning a key wasn't found in the database), there is no Length or Value.It just prints "(nil)".return 1: It tells the caller, "I only consumed 1 byte (the tag) to process this."*/
        case TAG_NIL:
            printf("(nil)\n");
            return 1;
        case TAG_ERR:
            if(size<1+8){           //An error message consists of 1 byte (Tag) + 4 bytes (Error Code) + 4 bytes (String Length). That's 9 bytes minimum. If we have less than 9 bytes, we can't safely read the header.
                msg("bad response");
                return -1;
            }
            {
                uint32_t code=0;
                uint32_t len=0; 
                memcpy(&code,&data[1],4);        //Copies the 4 bytes immediately after the tag (data[1]) into the integer code
                memcpy(&len,&data[1+4],4);        //copies the next 4 bytes (starting at index 5) into len
                if (size < 1 + 8 + len) { 
                     msg("bad response");
                    return -1; }  //Now that we know the string's length, we check if the total buffer is actually large enough to hold the Tag + Code + Length + the actual string data.
                printf("(err) %d %.*s\n", code, len, &data[1 + 8]); //Prints the error code and the string. %.*s is a clever C-style format specifier that prints exactly len characters from the &data[9] pointer, without needing a null-terminator \0
                return 1 + 8 + len;     //Returns the total number of bytes this whole error block consumed
            }
        case TAG_STR:
            if(size<1+4){
                msg("bad response");
                return -1;
            }
            {
                uint32_t len=0;
                memcpy(&len,&data[1],4);
                if(size<1+4+len){
                     msg("bad response");
                     return -1;
                }
                 printf("(str) %.*s\n", len, &data[1 + 4]);
                return 1 + 4 + len;

            }
        case TAG_INT:
             if(size<1+8){
                msg("bad response");
                return -1;
            }
            {
                int64_t val=0;
                memcpy(&val,&data[1],8);
                printf("(int) %ld\n", val);
                return 1 + 8;

            }
            case TAG_DBL:
            if (size < 1 + 8) {
                msg("bad response");
                return -1;
            }
            {
                double val = 0;
                memcpy(&val, &data[1], 8);
                printf("(dbl) %g\n", val); // %g prints a double cleanly
                return 1 + 8;
            }
        case TAG_ARR:
            {
                if(size<1+4){
                    msg("bad response");
                    return -1;
                }
                uint32_t len=0;
                size_t arr_bytes=1+4;         // Keeps a running tally of how many bytes we've processed so far. We start at 5 (Tag + Length).
                memcpy(&len,&data[1],4);    //Reads the number of elements in the array, not the number of bytes.
                // ---> THE MISSING LINE <---
                printf("(arr) len=%u\n", len);
                for(uint32_t i=0;i<len;i++){
                    int32_t rv=print_response(&data[arr_bytes],size-arr_bytes);   //It shifts the pointer forward by arr_bytes and passes the remaining buffer back into the print_response function.
                    if(rv<0){
                        return rv;
                    }
                    arr_bytes += (size_t)rv; //: It takes the number of bytes the nested call consumed (rv) and adds it to our running tally.
                }
                printf("(arr) end\n");
                return (int32_t)arr_bytes;   //inally, it returns the total size of the entire array (including all its nested items) so the caller knows how much memory was processed.
            }
        default:
            msg("bad response");          //If data[0] is a number we don't recognize (like 99), the server sent garbage data. We abort.
            return -1;
    }
}
static int32_t read_res(int fd) {
    // 4 bytes header
    char rbuf[4 + k_max_msg];
    errno = 0;
    int32_t err = read_full(fd, rbuf, 4);
    if (err) {
        if (errno == 0) {
            msg("EOF");
        } else {
            msg("read() error");
        }
        return err;
    }

    uint32_t len = 0;
    memcpy(&len, rbuf, 4);  // assume little endian
    if (len > k_max_msg) {
        msg("too long");
        return -1;
    }

    // reply body
    err = read_full(fd, &rbuf[4], len);
    if (err) {
        msg("read() error");
        return err;
    }

    // print the result
    /*we pass the memory address where the payload starts (&rbuf[4]) and the size (len) to your new recursive parser. We cast it to const uint8_t * because print_response expects unsigned bytes for binary data parsing. The function returns the total number of bytes it consumed into rv*/
    int32_t rv = print_response((uint8_t *)&rbuf[4], len);
    if (rv > 0 && (uint32_t)rv != len) {  /*This is a strict safety check. If the parser says "I successfully consumed 20 bytes" but the server originally told us the message was 30 bytes long (len), it means there is leftover, unparsed garbage data at the end of the buffer. We flag that as an error.*/
        msg("bad response");
        rv = -1;
    }
    return rv;
}
// static int32_t query(int fd, const char *text){
//     uint32_t len = (uint32_t)strlen(text);
//     if (len > k_max_msg) {
//         return -1;
//           /*Checks if the message is too big (over 4096 bytes). If it is, the function stops and returns -1 to avoid errors.*/
//     }
//     char wbuf[4 + k_max_msg];     /*Creates a buffer large enough for the header (4 bytes) and the maximum possible message.*/
//     memcpy(wbuf, &len, 4);   /*Copies the length of the message into the first 4 bytes of the buffer. This is the "Header."*/
//     memcpy(&wbuf[4], text, len);   /*Copies the actual text message into the buffer, starting right after the header (index 4).*/
//     if (int32_t err = write_all(fd, wbuf, 4 + len)) {
//         return err;
//     }
//     char rbuf[4 + k_max_msg];   /*A buffer to hold the incoming reply.*/
//     errno = 0;
//     int32_t err = read_full(fd, rbuf, 4);     /*Attempts to read exactly 4 bytes from the server*/
//     if (err) {
//         if (errno == 0) {
//             fprintf(stderr, "EOF\n");
//         } else {
//             fprintf(stderr, "read() error\n");
//         }
//         return err;
//     }
//     memcpy(&len, rbuf, 4);  // assume little endian
//     if (len > k_max_msg) {
//         fprintf(stderr, "too long\n");
//         return -1;
//     }
//     err = read_full(fd, &rbuf[4], len);
//     if (err) {
//         fprintf(stderr, "read() error\n");
//         return err;
//     }
//     printf("server says: %.*s\n", len, &rbuf[4]);
//     return 0;

// }  
/*This is the main function. It takes a file descriptor (fd) representing the connection to the server, 
 and a string (text) that we want to send.*/
  


int main(int argc, char **argv){
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    int rv = connect(fd, (const struct sockaddr *)&addr, sizeof(addr));
    if (rv) {
        die("connect");
    }

     std::vector<std::string> cmd;
    for (int i = 1; i < argc; ++i) {
        cmd.push_back(argv[i]);
    }
    int32_t err = send_req(fd, cmd);
    if (err) {
        goto L_DONE;
    }
    err = read_res(fd);
    if (err) {
        goto L_DONE;
    }

L_DONE:
    close(fd);
    return 0;
}

