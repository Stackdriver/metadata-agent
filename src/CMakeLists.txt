cmake_minimum_required(VERSION 2.6)
project(Metadata Agent)
add_executable(metadatad agent.cc api_server.cc base64.cc configuration.cc docker.cc environment.cc format.cc health_checker.cc instance.cc json.cc kubernetes.cc local_stream_delegate.cc local_stream_http.cc logging.cc metadatad.cc oauth2.cc reporter.cc resource.cc store.cc time.cc updater.cc)
