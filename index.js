var addon = require("./build/Release/itunes_db.node");

module.exports = {
	loadSync: function (db) {
		return addon.load(db);
	},

	load: function (db, cb) {
		// FIXME
		process.nextTick(function () {
			try {
				cb(undefined, addon.load(db));
			}
			catch (e) {
				cb(e);
			}
		});
	}
};
