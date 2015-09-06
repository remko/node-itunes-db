#include <node.h>
#include <node_buffer.h>
#include <iostream>
#include <libxml/parser.h>
#include <map>
#include <set>
#include <vector>
#include <nan.h>

using namespace v8;

namespace {
	class Parser {
		public:
			Parser(Local<Object>& library, Local<Array>& tracks, Local<Array>& playlists) : 
				library(library), tracks(tracks), playlists(playlists),
				level(0), inMediaDir(false), inTracks(false) {
			}

			void handleStartElement(const std::string&) {
				if (inPlaylists && level == 3) {
					currentPlaylistTracks = Nan::New<Array>();
				}

				level++;
				lastText = "";
			}

			void handleEndElement(const std::string& tag) {
				level--;
				if (level == 2) {
					if (inMediaDir && tag != "key") {
						Nan::Set(library, Nan::New("mediaDir").ToLocalChecked(), Nan::New(lastText).ToLocalChecked());
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

							Local<Object> track = Nan::New<Object>();
							for (DataMap::const_iterator i = data.begin(); i != data.end(); ++i) {
								Nan::Set(track, Nan::New(i->first).ToLocalChecked(), i->second);
							}
							Nan::Set(tracks, tracks->Length(), track);

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
							Nan::Set(currentPlaylistTracks, currentPlaylistTracks->Length(), i->second);
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
							Local<Object> playlist = Nan::New<Object>();
							for (DataMap::const_iterator i = data.begin(); i != data.end(); ++i) {
								Nan::Set(playlist, Nan::New(i->first).ToLocalChecked(), i->second);
							}
							Nan::Set(playlist, Nan::New("items").ToLocalChecked(), currentPlaylistTracks);
							Nan::Set(playlists, playlists->Length(), playlist);
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
					return Nan::New<Number>(strtoull(value.c_str(), 0, 10));
				}
				else if (type == "true") {
					return Nan::True();
				}
				else if (type == "false") {
					return Nan::False();
				}
				else if (type == "data") {
					Local<Value> data = Nan::New(value).ToLocalChecked();
					ssize_t bytes = Nan::DecodeBytes(data, Nan::BASE64);
					if (bytes < 0) {
						return Nan::New(value).ToLocalChecked();
					}
					char* buffer = new char[bytes];
					Nan::DecodeWrite(buffer, bytes, data, Nan::BASE64);
					return Nan::NewBuffer(buffer, bytes).ToLocalChecked();
				}
				else {
					return Nan::New(value).ToLocalChecked();
				}
			}
		
		private:
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


NAN_METHOD(load) {
	if (info.Length() < 1) {
		Nan::ThrowTypeError("Wrong number of arguments");
		return;
	}

	if (!info[0]->IsString()) {
		Nan::ThrowTypeError("Wrong arguments");
		return;
	}

	std::string db(*v8::String::Utf8Value(info[0]->ToString()));

	Local<Object> library = Nan::New<Object>();

	Local<Array> tracks = Nan::New<Array>();
	Nan::Set(library, Nan::New("tracks").ToLocalChecked(), tracks);

	Local<Array> playlists = Nan::New<Array>();
	Nan::Set(library, Nan::New("playlists").ToLocalChecked(), playlists);

	Parser parser(library, tracks, playlists);
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
		Nan::ThrowError("Parse error");
		return;
	}

	info.GetReturnValue().Set(library);
}

NAN_MODULE_INIT(InitAll) {
	Nan::Set(target, Nan::New<String>("load").ToLocalChecked(),
    Nan::GetFunction(Nan::New<FunctionTemplate>(load)).ToLocalChecked());
}

NODE_MODULE(itunes_db, InitAll)
