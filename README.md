# [node-itunes-db: Fast iTunes XML Parser](https://el-tramo.be/itunes-db)

An iTunes XML library database parser, designed for large libraries.

## Installation

    npm install itunes-db --save

## Usage

## API

### `load(db, cb)`

Loads the given iTunes database asynchronously.

**Warning:** This function currently blocks the main thread, and isn't therefore
really async.

- **`db`** - *string*  
    The path to the iTunes XML file to load.

- **`cb`** - *string*  
    The path to the iTunes XML file to load.

### `loadSync(db)`

Loads the given iTunes database synchronously.

- **`db`** - *string*  
    The path to the iTunes XML file to load.
