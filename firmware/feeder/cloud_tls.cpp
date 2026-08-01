#include "cloud_tls.h"

extern const uint8_t rootca_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t rootca_crt_bundle_end[] asm("_binary_x509_crt_bundle_end");

void configureCloudTransport(CloudTransport& client) {
  client.setCACertBundle(rootca_crt_bundle_start,
                         static_cast<size_t>(rootca_crt_bundle_end - rootca_crt_bundle_start));
  client.setHandshakeTimeout(15);
}
