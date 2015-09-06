var itunesDB = require("../index");
var path = require("path");
var test = require("tape");

test("loadSync", function (t) {
	var db = itunesDB.loadSync(path.join(__dirname, "testdb.xml"));

	t.equal(db.tracks.length, 2);

	t.equal(db.playlists.length, 2);
	t.equal(db.playlists[0].items.length, 0);
	t.equal(db.playlists[1].items.length, 2);

	t.end();
});
