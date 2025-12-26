/*
 * Copyright ©2025 Chris Thachuk & Naomi Alterman.  All rights reserved.
 * Permission is hereby granted to students registered for University of
 * Washington CSE 333 for use solely during Autumn Quarter 2025 for
 * purposes of the course.  No other use, copying, distribution, or
 * modification is permitted without prior written consent. Copyrights
 * for third-party components of this work must be honored.  Instructors
 * interested in reusing these course materials should contact the
 * author.
 */

#include <boost/algorithm/string.hpp>
#include <iostream>
#include <map>
#include <memory>
#include <vector>
#include <string>
#include <sstream>

#include "./FileReader.h"
#include "./HttpConnection.h"
#include "./HttpRequest.h"
#include "./HttpUtils.h"
#include "./HttpServer.h"
#include "./libhw3/QueryProcessor.h"

using std::cerr;
using std::cout;
using std::endl;
using std::list;
using std::map;
using std::string;
using std::stringstream;
using std::unique_ptr;

namespace hw4 {
///////////////////////////////////////////////////////////////////////////////
// Constants, internal helper functions
///////////////////////////////////////////////////////////////////////////////
// static
const int HttpServer::kNumThreads = 8;

static const char *kThreegleStr =
  "<html><head><title>333gle</title></head>\n"
  "<body>\n"
  "<center style=\"font-size:500%;\">\n"
  "<span style=\"position:relative;bottom:-0.33em;color:orange;\">3</span>"
    "<span style=\"color:red;\">3</span>"
    "<span style=\"color:gold;\">3</span>"
    "<span style=\"color:blue;\">g</span>"
    "<span style=\"color:green;\">l</span>"
    "<span style=\"color:red;\">e</span>\n"
  "</center>\n"
  "<p>\n"
  "<div style=\"height:20px;\"></div>\n"
  "<center>\n"
  "<form action=\"/query\" method=\"get\">\n"
  "<input type=\"text\" size=30 name=\"terms\" />\n"
  "<input type=\"submit\" value=\"Search\" />\n"
  "</form>\n"
  "</center><p>\n";

// This is the function that threads are dispatched into
// in order to process new client connections.
static void HttpServer_ThrFn(ThreadPool::Task *t);

// Given a request, produce a response.
static HttpResponse ProcessRequest(const HttpRequest &req,
                                   const string &base_dir,
                                   const list<string> &indices);

// Process a file request.
static HttpResponse ProcessFileRequest(const string &uri,
                                       const string &base_dir);

// Process a query request.
static HttpResponse ProcessQueryRequest(const string &uri,
                                        const list<string> &indices,
                                        const string &base_dir);

// Returns true if 's' starts with 'prefix'.
static bool StringStartsWith(const string &s, const string &prefix);

///////////////////////////////////////////////////////////////////////////////
// HttpServer
///////////////////////////////////////////////////////////////////////////////
bool HttpServer::Run(void) {
  // Create the server listening socket.
  int listen_fd;
  cout << "  creating and binding the listening socket..." << endl;
  if (!socket_.BindAndListen(AF_INET6, &listen_fd)) {
    cerr << endl << "Couldn't bind to the listening socket." << endl;
    return false;
  }

  // Spin, accepting connections and dispatching them.  Use a
  // threadpool to dispatch connections into their own thread.
  tp_.reset(new ThreadPool(kNumThreads));
  cout << "  accepting connections..." << endl << endl;
  while (!IsShuttingDown()) {
    // If the HST is successfully added to the threadpool, it'll (eventually)
    // get run and clean itself up.  But we need to manually delete it if
    // it doesn't get added.
    HttpServerTask *hst = new HttpServerTask(HttpServer_ThrFn, this);
    hst->base_dir = static_file_dir_path_;
    hst->indices = &indices_;

    if (!socket_.Accept(&hst->client_fd,
                        &hst->c_addr,
                        &hst->c_port,
                        &hst->c_dns,
                        &hst->s_addr,
                        &hst->s_dns)) {
      // The accept failed for some reason, so quit out of the server.  This
      // can happen when the `kill` command is used to shut down the server
      // instead of the more graceful /quitquitquit handler.
      delete hst;
      break;
    }

    // The accept succeeded; dispatch it to the workers.
    if (!tp_->Dispatch(hst)) {
      delete hst;
      break;
    }
  }
  return true;
}

void HttpServer::BeginShutdown() {
  Verify333(pthread_mutex_lock(&lock_) == 0);
  shutting_down_ = true;
  tp_->BeginShutdown();
  Verify333(pthread_mutex_unlock(&lock_) == 0);
}

bool HttpServer::IsShuttingDown() {
  bool retval;
  Verify333(pthread_mutex_lock(&lock_) == 0);
  retval = shutting_down_;
  Verify333(pthread_mutex_unlock(&lock_) == 0);
  return retval;
}

///////////////////////////////////////////////////////////////////////////////
// Internal helper functions
///////////////////////////////////////////////////////////////////////////////
static void HttpServer_ThrFn(ThreadPool::Task *t) {
  // Cast back our HttpServerTask structure with all of our new client's
  // information in it.
  unique_ptr<HttpServerTask> hst(static_cast<HttpServerTask*>(t));
  cout << "  client " << hst->c_dns << ":" << hst->c_port << " "
       << "(IP address " << hst->c_addr << ")" << " connected." << endl;

  // Read in the next request, process it, and write the response.
  
  hw4::HttpConnection conn(hst->client_fd);
  while (!hst->server_->IsShuttingDown()) {
    HttpRequest rq("/");
    // loop on GetNextRequest, break and return if false
    if (!conn.GetNextRequest(&rq)) {
      break;
    }

    // If the client requested the server to shut down, do so.
    if (StringStartsWith(rq.uri(), "/quitquitquit")) {
      hst->server_->BeginShutdown();
      break;
    }

    // build response using ProcessRequest and write it
    HttpResponse resp = ProcessRequest(rq, hst->base_dir, *(hst->indices));
    if (!conn.WriteResponse(resp)) {
      break;  // write failed
    }

    // if client sends a "Connection: close\r\n" header, break
    std::string connHdr = rq.GetHeaderValue("Connection");
    if (boost::iequals(connHdr, "close")) {
      break;
    }
  }
}

static HttpResponse ProcessRequest(const HttpRequest &req,
                                   const string &base_dir,
                                   const list<string> &indices) {
  // Is the user asking for a static file?
  if (StringStartsWith(req.uri(), "/static/")) {
    return ProcessFileRequest(req.uri(), base_dir);
  }

  // The user must be asking for a query.
  return ProcessQueryRequest(req.uri(), indices, base_dir);
}

static HttpResponse ProcessFileRequest(const string &uri,
                                       const string &base_dir) {
  // The response we'll build up.
  HttpResponse ret;

  string file_name = "";

  // 1. parse URI to extract relative file path
  URLParser parser;
  parser.Parse(uri);
  string path = parser.path();  // decoded path
  string prefix = "/static/";
  if (StringStartsWith(path, prefix)) {
    file_name = path.substr(prefix.size());  // relative path (file request)
  }

  string full_path = base_dir + "/" + file_name;
  // ensure the requested file is inside base_dir
  // directory traversal attack prevention
  if (!IsPathSafe(base_dir, full_path)) {
    ret.set_protocol("HTTP/1.1");
    ret.set_response_code(404);
    ret.set_message("Not Found");
    ret.set_content_type("text/html");
    ret.AppendToBody("<html><body>Couldn't find file \""
                     + EscapeHtml(file_name)
                     + "\"</body></html>\n");
    return ret;
  }

  // 2. use FileReader to read file into memory
  FileReader fr(base_dir, file_name);
  string contents;
  if (fr.ReadFile(&contents)) {
    // 3. copy file content into ret.body
    ret.AppendToBody(contents);

    // 4. determine content type based on file extension and
    // set the appropriate content type header
    string ct = "application/octet-stream";
    if (boost::iends_with(file_name, ".html") ||
        boost::iends_with(file_name, ".htm")) {
      ct = "text/html";
    } else if (boost::iends_with(file_name, ".txt")) {
      ct = "text/plain";
    } else if (boost::iends_with(file_name, ".css")) {
      ct = "text/css";
    } else if (boost::iends_with(file_name, ".js")) {
      ct = "application/javascript";
    } else if (boost::iends_with(file_name, ".xml")) {
      ct = "application/xml";
    } else if (boost::iends_with(file_name, ".png")) {
      ct = "image/png";
    } else if (boost::iends_with(file_name, ".gif")) {
      ct = "image/gif";
    } else if (boost::iends_with(file_name, ".jpg") ||
             boost::iends_with(file_name, ".jpeg")) {
      ct = "image/jpeg";
    }
    ret.set_content_type(ct);

    // success: set response code, protocol, message in an HttpResopnse
    ret.set_protocol("HTTP/1.1");
    ret.set_response_code(200);
    ret.set_message("OK");

    return ret;
  }


  // If you couldn't find the file, return an HTTP 404 error.
  ret.set_protocol("HTTP/1.1");
  ret.set_response_code(404);
  ret.set_message("Not Found");
  ret.AppendToBody("<html><body>Couldn't find file \""
                   + EscapeHtml(file_name)
                   + "\"</body></html>\n");
  return ret;
}

static HttpResponse ProcessQueryRequest(const string &uri,
                                        const list<string> &indices,
                                        const string &base_dir) {
  // The response we're building up.
  HttpResponse ret;

  // 1. HTML body: 333gle logo and search box
  stringstream body;
  body << kThreegleStr;

  // 3. parse uri for query arguments
  URLParser parser;
  parser.Parse(uri);
  auto args = parser.args();
  // create an iterator over the args map so that we can look for the "terms"
  // key, whose value is our search query
  string terms = "";
  auto it = args.find("terms");
  if (it != args.end()) {
    terms = it->second;
  }

  // trim whitespace
  boost::algorithm::trim(terms);
  // 3. parse query arguments themselves
  if (!terms.empty()) {
    std::vector<string> query;  // parsed tokens to feed into qp
    std::vector<string> tokens;  // raw pieces of search string
    // split terms on spaces into individual words
    // consecutive spaces treated as only one delimiter using token_compress_on
    boost::split(tokens, terms, boost::is_any_of(" "),
                 boost::token_compress_on);
    // loop thru each token, pushing non-empty tokens to lowercase into query
    for (auto &t : tokens) {
      if (!t.empty()) {
        query.push_back(boost::algorithm::to_lower_copy(t));
      }
    }

    // 4. initialize hw3 qp to process queries with our search indices
    hw3::QueryProcessor qp(indices);
    auto results = qp.ProcessQuery(query);

    // 2. display previously typed search results
    // EscapeHtml used to prevent XSS attacks
    body << "<div><b>Results for</b>: " << EscapeHtml(terms) << "</div>\n";
    body << "<ol>\n";

    // 5.
    for (const auto &r : results) {
      // convert absolute path to relative path under base_dir
      string rel = r.document_name;
      if (StringStartsWith(rel, base_dir + "/")) {
        rel = rel.substr(base_dir.size() + 1);
      }

      // hyperlink each result to its static file
      // EscapeHtml used to prevent XSS attacks
      body << "<li><a href=\"/static/" << EscapeHtml(rel) << "\">"
           << EscapeHtml(r.document_name) << "</a> "
           << "(rank " << r.rank << ")</li>\n";
    }
    body << "</ol>\n";
  }

  // close html body, attach to response
  body << "</body></html>\n";
  ret.AppendToBody(body.str());

  // set protocol, code, message, and content type.
  ret.set_protocol("HTTP/1.1");
  ret.set_response_code(200);
  ret.set_message("OK");
  ret.set_content_type("text/html");

  return ret;
}

static bool StringStartsWith(const string &s, const string &prefix) {
  return s.substr(0, prefix.size()) == prefix;
}


}  // namespace hw4
