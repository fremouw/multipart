# multipart

Simple multipart parser for ESP8266 using ESP8266_RTOS_SDK. 

Note: you need to have all post data upfront, the multipart_post_t struct only contains pointers to the original data; not delimited with \0.

## Example

```
multipart_parse_context_t context;

multipart_init(&context, header_data);

multipart_post_t post;
char data[512];
char* next = NULL;

// Fill data with raw post data.

esp_err_t err = multipart_parse(&context, data, &post, next);
if(err == ESP_OK) {
  printf("parsed LEN=%d, DATA=%.*s<-\r\n", post.data_len, post.data_len, post.data);
}

if(next != NULL) {
  printf("more parts to come.\r\n");
}
```
