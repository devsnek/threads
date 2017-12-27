# threads

These are real threads that don't crash **and** run your js without exploding.

TODO:

- [X] Console in thread
- [X] Buffer in thread (freoss/buffer)
- [ ] Allow passing "references" instead of copies


---


# Docs

<a name="Thread"></a>

## Thread
**Kind**: global class

* [Thread](#Thread)
    * [new Thread(fn, ...props)](#new_Thread_new)
    * _instance_
        * [.send(value)](#Thread+send)
        * [.join(handler)](#Thread+join)
        * [.catch(handler)](#Thread+catch)
        * [.terminate()](#Thread+terminate)
        * [.lock()](#Thread+lock) ⇒ <code>boolean</code>
        * [.unlock()](#Thread+unlock)
    * _inner_
        * [~Context](#Thread..Context) : <code>Object</code>
        * [~fnCallback](#Thread..fnCallback) : <code>function</code>

<a name="new_Thread_new"></a>

### new Thread(fn, ...props)
Create a thread


| Param | Type | Description |
| --- | --- | --- |
| fn | [<code>fnCallback</code>](#Thread..fnCallback) | Function that will run in a new thread |
| ...props | <code>\*</code> | Values to pass to the thread callback |

<a name="Thread+send"></a>

### thread.send(value)
Send a value to the thread

**Kind**: instance method of [<code>Thread</code>](#Thread)

| Param | Type | Description |
| --- | --- | --- |
| value | <code>\*</code> | Value to send |

<a name="Thread+join"></a>

### thread.join(handler)
Wait for the thread to finish and call `handler` with its return value.

**Kind**: instance method of [<code>Thread</code>](#Thread)

| Param | Type | Description |
| --- | --- | --- |
| handler | <code>function</code> | Callback |

<a name="Thread+catch"></a>

### thread.catch(handler)
Handle any errors that happen during execution of the thread's code

**Kind**: instance method of [<code>Thread</code>](#Thread)

| Param | Type | Description |
| --- | --- | --- |
| handler | <code>function</code> | Callback |

<a name="Thread+terminate"></a>

### thread.terminate()
Terminate the thread

**Kind**: instance method of [<code>Thread</code>](#Thread)
<a name="Thread+lock"></a>

### thread.lock() ⇒ <code>boolean</code>
Lock the thread's context's mutex, analogous to std::mutex::try_lock

**Kind**: instance method of [<code>Thread</code>](#Thread)
**Returns**: <code>boolean</code> - If the lock was successfully obtained
<a name="Thread+unlock"></a>

### thread.unlock()
Unlock the thread context's mutex, analogous to std::mutex::unlock

**Kind**: instance method of [<code>Thread</code>](#Thread)
<a name="Thread..Context"></a>

### Thread~Context : <code>Object</code>
**Kind**: inner typedef of [<code>Thread</code>](#Thread)
**Properties**

| Name | Type |
| --- | --- |
| on | <code>function</code> |
| send | <code>function</code> |
| terminate | <code>function</code> |
| lock | <code>function</code> |
| unlock | <code>function</code> |

<a name="Thread..fnCallback"></a>

### Thread~fnCallback : <code>function</code>
**Kind**: inner typedef of [<code>Thread</code>](#Thread)

| Param | Type | Description |
| --- | --- | --- |
| ...args | <code>args</code> | Arguments from the [Thread](#Thread) constructor |
| context | [<code>Context</code>](#Thread..Context) |  |
