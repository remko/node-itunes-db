#include <node.h>
#include <node_buffer.h>
#include <iostream>
#include <libxml/parser.h>
#include <map>
#include <set>
#include <vector>

using namespace v8;

namespace {
	class Parser {
		public:
			Parser(Isolate* isolate, Local<Object>& library, Local<Array>& tracks, Local<Array>& playlists) : 
				isolate(isolate), library(library), tracks(tracks), playlists(playlists),
				level(0), inMediaDir(false), inTracks(false) {
			}

			void handleStartElement(const std::string&) {
				if (inPlaylists && level == 3) {
					currentPlaylistTracks = Array::New(isolate);
				}

				level++;
				lastText = "";
			}

			void handleEndElement(const std::string& tag) {
				level--;
				if (level == 2) {
					if (inMediaDir && tag != "key") {
						library->Set(String::NewFromUtf8(isolate, "mediaDir"), String::NewFromUtf8(isolate, lastText.c_str()));
					}
					inTracks = tag == "key" && lastText == "Tracks";
					inPlaylists = tag == "key" && lastText == "Playlists";
					inMediaDir = tag == "key" && lastText == "Music Folder";
				}
				if (inTracks) {
					if (level == 4) {
						if (tag == "key") {
							currentKey = lastText;
						}
						else if (!currentKey.empty()) {
							data[currentKey] = createValue(tag, lastText);
						}
					}
					else if (level == 3) {
						if (tag == "key") {
							currentTrackID = lastText;
						}
						else if (data.size()) {
							Local<Value> persistentID = data.at("Persistent ID");
							std::string id(*v8::String::Utf8Value(persistentID->ToString()));
							int wasInserted = ids.insert(id).second;
							if (!wasInserted) {
								std::cerr << "Duplicate with ID" << id << std::endl;
								exit(-1);
							}

							Local<Object> track = Object::New(isolate);
							for (DataMap::const_iterator i = data.begin(); i != data.end(); ++i) {
								track->Set(String::NewFromUtf8(isolate, i->first.c_str()), i->second);
							}
							tracks->Set(tracks->Length(), track);

							tracksByID[currentTrackID] = track;

							data.clear();
						}
					}
				}
				if (inPlaylists) {
					if (level == 6) {
						if (tag != "key") {
							TracksByIDMap::const_iterator i = tracksByID.find(lastText);
							if (i == tracksByID.end()) {
								std::cerr << "Unable to find track " << lastText << std::endl;
								exit(-1);
							}
							currentPlaylistTracks->Set(currentPlaylistTracks->Length(), i->second);
						}
					}
					else if (level == 4) {
						if (tag == "key") {
							currentKey = lastText;
						}
						else if (!currentKey.empty() && currentKey != "Playlist Items") {
							data[currentKey] = createValue(tag, lastText);
						}
					}
					else if (level == 3) {
						if (data.size()) {
							Local<Object> playlist = Object::New(isolate);
							for (DataMap::const_iterator i = data.begin(); i != data.end(); ++i) {
								playlist->Set(String::NewFromUtf8(isolate, i->first.c_str()), i->second);
							}
							playlist->Set(String::NewFromUtf8(isolate, "items"), currentPlaylistTracks);
							playlists->Set(playlists->Length(), playlist);
							data.clear();
						}
					}
				}
				lastText = "";
			}

			void handleCharacterData(const std::string& text) {
				lastText = lastText + text;
			}

		private:
			Local<Value> createValue(const std::string& type, const std::string& value) {
				if (type == "integer") {
					return Number::New(isolate, strtoull(value.c_str(), 0, 10));
				}
				else if (type == "true") {
					return Boolean::New(isolate, true);
				}
				else if (type == "false") {
					return Boolean::New(isolate, false);
				}
				else if (type == "data") {
					return node::Buffer::New(isolate, String::NewFromUtf8(isolate, value.c_str()), node::BASE64);
				}
				else {
					return String::NewFromUtf8(isolate, value.c_str());
				}
			}
		
		private:
			Isolate* isolate;
			Local<Object>& library;
			Local<Array>& tracks;
			Local<Array>& playlists;

			int level;
			std::string lastText;
			bool inMediaDir;
			bool inTracks;
			bool inPlaylists;
			std::string currentKey;
			std::string currentTrackID;
			Local<Array> currentPlaylistTracks;

			typedef std::map<std::string, Local<Value> > DataMap;
			DataMap data;

			typedef std::map<std::string, Local<Object> > TracksByIDMap;
			TracksByIDMap tracksByID;

			// for verification only
			std::set<std::string> ids;
	};
}

static void handleStartElement(void* parser, const xmlChar* name, const xmlChar*, const xmlChar*, int, const xmlChar**, int, int, const xmlChar**) {
	static_cast<Parser*>(parser)->handleStartElement(reinterpret_cast<const char*>(name));
}

static void handleEndElement(void *parser, const xmlChar* name, const xmlChar*, const xmlChar*) {
	static_cast<Parser*>(parser)->handleEndElement(reinterpret_cast<const char*>(name));
}

static void handleCharacterData(void* parser, const xmlChar* data, int len) {
	static_cast<Parser*>(parser)->handleCharacterData(std::string(reinterpret_cast<const char*>(data), static_cast<size_t>(len)));
}

static void handleError(void*, const char* /*m*/, ... ) {
	/*
	va_list args;
	va_start(args, m);
	vfprintf(stdout, m, args);
	va_end(args);
	*/
}

static void handleWarning(void*, const char*, ... ) {
}


void load(const FunctionCallbackInfo<Value>& args) {
	Isolate* isolate = Isolate::GetCurrent();
	HandleScope scope(isolate);

	if (args.Length() < 1) {
		isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Wrong number of arguments")));
		return;
	}

	if (!args[0]->IsString()) {
		isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Wrong arguments")));
		return;
	}

	std::string db(*v8::String::Utf8Value(args[0]->ToString()));

	Local<Object> library = Object::New(isolate);

	Local<Array> tracks = Array::New(isolate);
	library->Set(String::NewFromUtf8(isolate, "tracks"), tracks);

	Local<Array> playlists = Array::New(isolate);
	library->Set(String::NewFromUtf8(isolate, "playlists"), playlists);

	Parser parser(isolate, library, tracks, playlists);
	xmlInitParser();
	xmlSAXHandler handler;
	memset(&handler, 0, sizeof(handler));
	handler.initialized = XML_SAX2_MAGIC;
	handler.startElementNs = &handleStartElement;
	handler.endElementNs = &handleEndElement;
	handler.characters = &handleCharacterData;
	handler.warning = &handleWarning;
	handler.error = &handleError;
	if (xmlSAXUserParseFile(&handler, &parser, db.c_str()) != XML_ERR_OK) {
		isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Parse error")));
		return;
	}

	// args.GetReturnValue().Set(String::NewFromUtf8(isolate, "world"));
	args.GetReturnValue().Set(library);
}

void init(Handle<Object> exports) {
	NODE_SET_METHOD(exports, "load", load);
}

NODE_MODULE(addon, init)
