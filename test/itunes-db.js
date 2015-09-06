var itunesDB = require("../index");
var path = require("path");
var test = require("tape");

function checkTestDB(t, db) {
	t.equal(db.tracks.length, 2);

	t.equal(db.playlists.length, 2);
	t.equal(db.playlists[0].items.length, 0);
	t.equal(db.playlists[1].items.length, 2);
}

test("loadSync - testdb.xml", function (t) {
	var db = itunesDB.loadSync(path.join(__dirname, "data", "testdb.xml"));
	// console.log(db);
	checkTestDB(t, db);

	t.end();
});

test("loadSync - nonexistent file", function (t) {
	t.throws(function () {
		itunesDB.loadSync(path.join(__dirname, "doesnt_exist.xml"));
	}, Error);

	t.end();
});

test("loadSync - invalid xml", function (t) {
	t.throws(function () {
		itunesDB.loadSync(path.join(__dirname, "data", "invalid_xml.xml"));
	}, Error);

	t.end();
});

test("load - testdb.xml", function (t) {
	itunesDB.load(path.join(__dirname, "data", "testdb.xml"), function (err, db) {
		t.false(err);
		checkTestDB(t, db);

		t.end();
	});
});

test("load - nonexistent file", function (t) {
	itunesDB.load(path.join(__dirname, "doesnt_exist.xml"), function (err) {
		t.assert(err);
		t.assert(err instanceof Error);

		t.end();
	});
});

test("load - invalid xml", function (t) {
	itunesDB.load(path.join(__dirname, "data", "invalid_xml.xml"), function (err) {
		t.assert(err);
		t.assert(err instanceof Error);

		t.end();
	});
});
