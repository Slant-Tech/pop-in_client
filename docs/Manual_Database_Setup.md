<!--
	SPDX-FileCopyrightText: 2022 Slant Tech, Dylan Wadler <dwadler@slant.tech>
 
	SPDX-License-Identifier: GFDL-1.3-or-later
-->

# Manual Database Setup

After setting up the docker container with docker-compose, there are a few required data structures 

The most important is the dbinfo structure. This is what tells a client what parts and projects are available, along with some other data.

The json format is as follows:

```json
{
    "version": {
        "major": 0,
        "minor": 1,
        "patch": 0
    },
    "nprj": 0,
    "nbom": 0,
    "init": 1,
    "lock": 0,
    "ptypes": [
        {
            "type": "misc",
            "npart": 0
        }
    ],
    "invs":[
        {
            "name": "Replace-with-name",
            "loc": 0
        }
    ]

}
```

Enter the above data into a new JSON key in the redis database. The easiest way to do this would be using Redis Insight, which should have been started with the docker container.

The next step is to initialize the search functions. There are only 3 commands to enter, which can also be done with Redis Insight.

Navigate to the notepad button on the left side of the page, called 'Workbench'

Then in the editor, type each command and press the green play button. Doing one at a time would be ideal.

```
FT.SEARCH partid ON JSON PREFIX 1 part: SCHEMA $.mpn AS mpn TEXT SORTABLE $.ipn AS ipn NUMERIC
FT.SEARCH prjid ON JSON PREFIX 1 prj: SCHEMA $.name AS name TEXT SORTABLE $.ipn AS ipn NUMERIC
FT.SEARCH bomid ON JSON PREFIX 1 bom: SCHEMA $.name AS name TEXT SORTABLE $.ipn AS ipn NUMERIC
```
This will create the indexes for searching for the various database objects
