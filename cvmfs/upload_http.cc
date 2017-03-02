/**
 * This file is part of the CernVM File System.
 */

#include "upload_http.h"

#include <limits>
#include <vector>

#include "file_processing/char_buffer.h"
#include "util/string.h"

namespace {

void LogBadConfig(const std::string& config) {
  LogCvmfs(kLogUploadHttp, kLogStderr,
           "Failed to parse spooler configuration string '%s'.\n"
           "Provide: http://<repository_services_url>:<port>[/<api_root>]",
           config.c_str());
}
}

namespace upload {

HttpStreamHandle::HttpStreamHandle(const CallbackTN* commit_callback,
                                   ObjectPack::BucketHandle bkt)
    : UploadStreamHandle(commit_callback), bucket(bkt) {}

bool HttpUploader::WillHandle(const SpoolerDefinition& spooler_definition) {
  return spooler_definition.driver_type == SpoolerDefinition::HTTP;
}

bool HttpUploader::ParseSpoolerDefinition(
    const SpoolerDefinition& spooler_definition, HttpUploader::Config* config) {
  const std::string& config_string = spooler_definition.spooler_configuration;
  if (!config) {
    LogCvmfs(kLogUploadHttp, kLogStderr, "\"config\" argument is NULL");
    return false;
  }

  if (spooler_definition.session_token_file.empty()) {
    LogCvmfs(kLogUploadHttp, kLogStderr,
             "Failed to configure HTTP uploader. "
             "Missing session token file.\n");
    return false;
  }
  config->session_token_file = spooler_definition.session_token_file;

  if (!HasPrefix(config_string, "http", false) || config_string.length() <= 7) {
    LogBadConfig(config_string);
    return false;
  }

  // Repo address, e.g. http://my.repo.address
  config->api_url = config_string;

  return true;
}

HttpUploader::HttpUploader(const SpoolerDefinition& spooler_definition)
    : AbstractUploader(spooler_definition), config_(), session_context_() {
  assert(spooler_definition.IsValid() &&
         spooler_definition.driver_type == SpoolerDefinition::HTTP);

  if (!ParseSpoolerDefinition(spooler_definition, &config_)) {
    abort();
  }

  atomic_init32(&num_errors_);

  LogCvmfs(kLogUploadHttp, kLogStderr,
           "HTTP uploader configuration:\n"
           "  API URL: %s\n"
           "  Session token file: %s\n",
           config_.api_url.c_str(), config_.session_token_file.c_str());
}

HttpUploader::~HttpUploader() {}

bool HttpUploader::Initialize() {
  if (!AbstractUploader::Initialize()) {
    return false;
  }
  std::string session_token;
  if (!ReadSessionTokenFile(config_.session_token_file, &session_token)) {
    return false;
  }
  return session_context_.Initialize(config_.api_url, session_token);
}

bool HttpUploader::FinalizeSession() {
  return session_context_.FinalizeSession();
}

std::string HttpUploader::name() const { return "HTTP"; }

bool HttpUploader::Remove(const std::string& /*file_to_delete*/) {
  return false;
}

bool HttpUploader::Peek(const std::string& /*path*/) const { return false; }

bool HttpUploader::PlaceBootstrappingShortcut(
    const shash::Any& /*object*/) const {
  return false;
}

unsigned int HttpUploader::GetNumberOfErrors() const {
  return atomic_read32(&num_errors_);
}

void HttpUploader::FileUpload(const std::string& /*local_path*/,
                              const std::string& /*remote_path*/,
                              const CallbackTN* /*callback*/) {}

UploadStreamHandle* HttpUploader::InitStreamedUpload(
    const CallbackTN* callback) {
  return new HttpStreamHandle(callback, session_context_.NewBucket());
}

void HttpUploader::StreamedUpload(UploadStreamHandle* handle,
                                  CharBuffer* buffer,
                                  const CallbackTN* callback) {
  if (!buffer->IsInitialized()) {
    LogCvmfs(kLogUploadHttp, kLogStderr,
             "Streamed upload - input buffer is not initialized");
    Respond(callback, UploaderResults(1, buffer));
    return;
  }

  HttpStreamHandle* hd = dynamic_cast<HttpStreamHandle*>(handle);
  if (!hd) {
    LogCvmfs(kLogUploadHttp, kLogStderr,
             "Streamed upload - incompatible upload handle");
    Respond(callback, UploaderResults(2, buffer));
    return;
  }

  ObjectPack::AddToBucket(buffer->ptr(), buffer->used_bytes(), hd->bucket);

  Respond(callback, UploaderResults(0, buffer));
}

void HttpUploader::FinalizeStreamedUpload(UploadStreamHandle* handle,
                                          const shash::Any& content_hash) {
  HttpStreamHandle* hd = dynamic_cast<HttpStreamHandle*>(handle);
  if (!hd) {
    LogCvmfs(kLogUploadHttp, kLogStderr,
             "Finalize streamed upload - incompatible upload handle");
    Respond(handle->commit_callback, UploaderResults(2));
    return;
  }

  if (!session_context_.CommitBucket(ObjectPack::kCas, content_hash, hd->bucket,
                                     "")) {
    LogCvmfs(kLogUploadHttp, kLogStderr,
             "Finalize streamed upload - could not commit bucket");
    Respond(handle->commit_callback, UploaderResults(4));
    return;
  }

  Respond(handle->commit_callback, UploaderResults(0));
}

bool HttpUploader::ReadSessionTokenFile(const std::string& token_file_name,
                                        std::string* token) {
  if (!token) {
    return false;
  }

  FILE* token_file = std::fopen(token_file_name.c_str(), "r");
  if (!token_file) {
    LogCvmfs(kLogUploadHttp, kLogStderr,
             "HTTP Uploader - Could not open session token "
             "file. Aborting.");
    return false;
  }

  return GetLineFile(token_file, token);
}

void HttpUploader::BumpErrors() const { atomic_inc32(&num_errors_); }

}  // namespace upload
