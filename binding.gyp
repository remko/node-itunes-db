{
	"targets": [
		{
			"target_name": "itunes_db",
			"sources": [ "itunes-db.cpp" ],
			"libraries": [
				"<!@(xml2-config --libs)"
			],
			"include_dirs": [
				"<!(node -e \"require('nan')\")",
				"<!@(xml2-config --cflags | sed -e s/-I//)"
			]
		}
	]
}
