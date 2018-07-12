# CreDB C++ Documentation

As you probably know by know, CreDB is a high-integrity datastore with an API that is similar to MongoDB.

This detailed documentation wants to cover, in particular, the four main features not found in other datastores:
policy enforcement, timeline inspection, witness generation, and protected function evaluation.

## Lesson 1: First steps
In this lesson you'll learn how to connect to a server. Further we'll talk about issuing basic operations such as reading or writing values.
For the following, we will assume that a server is running locally and can be connected to.

A client connection is set up using the credb::create_client call. Note that the return value is managed by a shared pointer and does not need manual clean up.

```cpp
#include <credb/Client.h>

int main()
{
    auto conn = credb::create_client("test", "testserver", "localhost");

    // The rest of the code will go here

    return 0;
}
```

Objects on the server side a stored inside in collections. Collections are similar to tables in SQL systems and give the data more structure. 
To work with the data we first have to grab a handle to one of the collections, which exposes the credb::Collection interface.
Note that by writing an object, you may create the collection if it doesn't exist yet.
As CreDB stores JSON objects, there is no need to define a schema in advance.

```cpp
auto c = conn->get_collection("default");
int in_val = 42;

c.put("foo", json::Integer(in_val));

int out_val = c.get("foo").as_integer();

/// At this point in_val == out_val 
```

## Lesson 2: Search and Secondary Indexes
Like other datastores, CreDB supports searching for objects matching specific predicates. Searches can be invoked using the credb::Collection::find and credb::Collection::find_one commands.

Lets say we have stored a two JSON objects like so:

```cpp
c.put("jane", json::Document("{\"full name\": \"Jane Doe\", \"hometown\": \"Ithaca\"}"));
c.put("jon",  json::Document("{\"full name\": \"Jon Doe\",  \"hometown\": \"Trumansburg\"}"));
```

We can then query them not only by their primary keys `jane` and `jon` but also by any of the fields in the JSON document.

```cpp
// Find one netry named Jane Doe
auto jane = c.find_one(json::Document("{\"full name\": \"Jane Doe\"}"))

// Find everybody from Ithaca and Trumansburg
auto result = c.find(json::Document("{\"hometown\": {\"$in\": [\"Ithaca\", \"Trumansburg\"]}}"));

// Iterate results
for(auto &[key, value]: result)
{
    // Print to stdout
    std::cout << key << ": " << value.pretty_str() << std::endl;
}
```

Search will perform a linear scan over all entries, which can be slow if your dataset is large. 
Luckily, CreDB also support secondary indexes that can speed up queries on commonly used fields. 

Currently, our indexes only support equality operations. All you need to do to create them is give them a unique name and specifies the field(s) to be indexed.
The credb::Collection::create_index function can be used for this.

```cpp
c.create_index("hometown_index", {"hometown"});
```

## Lesson 3: Timeline inspection
After data has been modified by one or multiple clients (such as in the previous lesson), we can leverage CreDB's immutable timeline to inspect the changes that have been made.

```cpp
auto c = conn->get_collection("default");

auto vec = c->get_history("foo");
int i = 1;

// Print history to standard output
for(auto &v: vec)
{
    std::cout << "version #" << (++i) << ": " << v.pretty_str() << std::endl;
}
```

## Lesson 4: Policies
Coming soon.
