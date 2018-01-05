#pragma once

#include <cowlang/Value.h>
#include <functional>
#include <json/Document.h>
#include <vector>

#include "Witness.h"
#include "event_id.h"

namespace credb
{

/**
 * Handle to access and modify a collection
 * A collection corresponds to a "table" in RDBMs
 */
class Collection
{
public:
    virtual ~Collection() = default;

    /**
     * @label{Collection_diff}
     * @brief Get the changes between two versions of the same object
     */
    virtual std::vector<json::Document> diff(const std::string &key, version_number_t version1, version_number_t version2) = 0;

    /**
     * @label{Collection_create_index}
     *
     * Create a secondary index for this collection
     * This index will cover all objects containing the paths specified.
     *
     * @param name
     *      The identifier of the index. Must be unique for this collection
     * @param paths
     *      The paths the index will cover
     */
    virtual bool create_index(const std::string &name, const std::vector<std::string> &paths) = 0;

    /**
     * @label{Collection_call}
     *
     * Call a function that is stored as an object on the server
     *
     * If the function executes successfully the return value of this call will be the return value of the serve side function.
     * Otherwise, this call will throw an exception.
     */
    virtual cow::ValuePtr call(const std::string &program_name, const std::vector<std::string> &args) = 0;

    /**
     * @label{Collection_drop_index}
     *
     * Remove the specified secondary index
     *
     * @param name
     *      The identifier of the index to be removed
     */
    virtual bool drop_index(const std::string &name) = 0;

    /**
     * @label{Collection_name}
     *
     * What is the name of this collection?
     */
    virtual const std::string &name() = 0;

    /**
     * @label{Collection_size}
     *
     * How many objects are there in this collection?
     */
    virtual uint32_t size() { return count(); }

    /**
     * @label{Collection_set_trigger}
     *
     * @brief
     *      Set a function that will be triggered when the collection is modified,
     *      This will overwrite any existing trigger for this collection
     *
     * @note
     *      Make sure this function does not go out of scope before you unset the trigger!
     */
    virtual bool set_trigger(std::function<void()> lambda) = 0;

    /**
     * @label{Collection_unset_trigger}
     * @brief Unset modification notifications for this collection
     */
    virtual bool unset_trigger() = 0;

    /**
     * @label{Collection_count}
     * @brief Return the number of objects in this collection matching a specified predicate
     */
    virtual uint32_t count(const json::Document &predicates = json::Document("")) = 0;

    /**
     * @label{Collection_add}
     * @brief Increment the vale of an object
     * @param path
     *       The path of the object. Should be of form "name[.field]*"
     * @param doc
     *       The value to be incremented with. Should either be numeric or string (strings will be
     * appened)
     */
    virtual event_id_t add(const std::string &path, const json::Document &doc) = 0;

    /**
     * @label{Collection_put}
     * @brief Insert a new value and generate a unique key for it
     */
    virtual std::tuple<std::string, event_id_t> put(const json::Document &doc) = 0;

    /**
     * @label{Collection_put}
     * @brief Insert or update the value of an object
     *
     * @param key
     *      The path of the object. Should be of form "name[.field]*"
     * @param doc
     *      The value to be stored
     */
    virtual event_id_t put(const std::string &key, const json::Document &doc) = 0;

    /**
     * @label{Collection_put_code}
     * @brief Create a new object holding executable code
     *
     * @param key
     *      The key to store the new object in
     * @param code
     *      Source code of the code to be stored. Will be compiled to byte code on the client side.
     */ 
    virtual event_id_t put_code(const std::string &key, const std::string &code) = 0;

    /**
     * @label{Collection_put_code_from_file}
     * @brief Create a new object holding executable code loaded from a specified file
     *
     * @param key
     *      The key to store the new object in
     * @param filename
     *      Name of the file that contains the sourcecode
     */
    virtual event_id_t put_code_from_file(const std::string &key, const std::string &filename) = 0;

    /**
     * @label{Collection_put_from_file}
     * @brief Insert or update an object to store the contents in a file
     *
     * @param key
     *      The identifier of the object
     * @param filename
     *      Name of the file containing the JSON data
     */
    virtual event_id_t put_from_file(const std::string &key, const std::string &filename) = 0;

    /**
     * @label{Collection_clear}
     * Remove all objects stored in this collection
     */
    virtual bool clear() = 0;

    /**
     * @label{Collection_remove}
     * @brief Remove an object from the collection
     *
     * @param key
     *      The identifier of the object to be removed
     */
    virtual event_id_t remove(const std::string &key) = 0;

    /**
     * @brief Get a value of an object
     *
     * @param key
     *      The primary key of the object
     * @param event_id [out]
     *      The event identifier of the most recent version of the object
     */
    virtual json::Document get(const std::string &key, event_id_t &event_id) = 0;

    /**
     * Check whether an object exists
     */
    virtual bool has_object(const std::string &key) = 0;

    /**
     * @brief Get a value and return a witness for it
     */
    virtual json::Document get_with_witness(const std::string &key, event_id_t &event_id, Witness &witness) = 0;

    /**
     * @label{Collection_get_history}
     * @brief Get the history of changes to a specified object
     */
    virtual std::vector<json::Document> get_history(const std::string &key) = 0;

    /**
     * @label{Collection_find_one}
     * @brief returns the first object that fits a predicate
     *
     * @param predicates [optional]
     *     the object has to match all specified predicates
     * @param projection [optional]
     *     only return the fields specified
     */
    virtual std::tuple<std::string, json::Document>
    find_one(const json::Document &predicates = json::Document(""),
             const std::vector<std::string> &projection = {}) = 0;

    /**
     * @label{Collection_find}
     * @brief find all objects that fit a predicate
     *
     * @param predicates [optional]
     *     the object has to match all specified predicates. if no predicate is specified all objects in the collection are returned.
     * @param projection [optional]
     *     only return the fields specified
     * @param limit [optional]
     *     only return up to a certain number of objects
     */
    virtual std::vector<std::tuple<std::string, json::Document>>
    find(const json::Document &predicates = json::Document(""),
         const std::vector<std::string> &projection = {},
         int32_t limit = -1) = 0;

    /**
     * @label{Collection_get}
     * @brief Get a value of an object
     *
     * @param key
     *      The primary key of the object
     */
    json::Document get(const std::string &key)
    {
        event_id_t eid;
        return std::forward<json::Document>(get(key, eid));
    }
};

} // namespace credb
