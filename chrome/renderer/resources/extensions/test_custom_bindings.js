// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// test_custom_bindings.js
// mini-framework for ExtensionApiTest browser tests

var binding = require('binding').Binding.create('test');

var chrome = requireNative('chrome').GetChrome();
var GetExtensionAPIDefinitionsForTest =
    requireNative('apiDefinitions').GetExtensionAPIDefinitionsForTest;
var GetAvailability = requireNative('v8_context').GetAvailability;
var GetAPIFeatures = requireNative('test_features').GetAPIFeatures;

binding.registerCustomHook(function(api) {
  var chromeTest = api.compiledApi;
  var apiFunctions = api.apiFunctions;

  chromeTest.tests = chromeTest.tests || [];

  var currentTest = null;
  var lastTest = null;
  var testsFailed = 0;
  var testCount = 1;
  var failureException = 'chrome.test.failure';

  // Helper function to get around the fact that function names in javascript
  // are read-only, and you can't assign one to anonymous functions.
  function testName(test) {
    return test ? (test.name || test.generatedName) : "(no test)";
  }

  function testDone() {
    // Use setTimeout here to allow previous test contexts to be
    // eligible for garbage collection.
    setTimeout(chromeTest.runNextTest, 0);
  }

  function allTestsDone() {
    if (testsFailed == 0) {
      chromeTest.notifyPass();
    } else {
      chromeTest.notifyFail('Failed ' + testsFailed + ' of ' +
                             testCount + ' tests');
    }

    // Try to get the script to stop running immediately.
    // This isn't an error, just an attempt at saying "done".
    throw "completed";
  }

  var pendingCallbacks = 0;

  apiFunctions.setHandleRequest('callbackAdded', function() {
    pendingCallbacks++;

    var called = null;
    return function() {
      if (called != null) {
        var redundantPrefix = 'Error\n';
        chrome.test.fail(
          'Callback has already been run. ' +
          'First call:\n' +
          $String.slice(called, redundantPrefix.length) + '\n' +
          'Second call:\n' +
          $String.slice(new Error().stack, redundantPrefix.length));
      }
      called = new Error().stack;

      pendingCallbacks--;
      if (pendingCallbacks == 0) {
        chromeTest.succeed();
      }
    };
  });

  apiFunctions.setHandleRequest('runNextTest', function() {
    // There may have been callbacks which were interrupted by failure
    // exceptions.
    pendingCallbacks = 0;

    lastTest = currentTest;
    currentTest = chromeTest.tests.shift();

    if (!currentTest) {
      allTestsDone();
      return;
    }

    try {
      chromeTest.log("( RUN      ) " + testName(currentTest));
      currentTest.call();
    } catch (e) {
      if (e !== failureException)
        chromeTest.fail('uncaught exception: ' + e);
    }
  });

  apiFunctions.setHandleRequest('fail', function(message) {
    chromeTest.log("(  FAILED  ) " + testName(currentTest));

    var stack = {};
    Error.captureStackTrace(stack, chromeTest.fail);

    if (!message)
      message = "FAIL (no message)";

    message += "\n" + stack.stack;
    console.log("[FAIL] " + testName(currentTest) + ": " + message);
    testsFailed++;
    testDone();

    // Interrupt the rest of the test.
    throw failureException;
  });

  apiFunctions.setHandleRequest('succeed', function() {
    console.log("[SUCCESS] " + testName(currentTest));
    chromeTest.log("(  SUCCESS )");
    testDone();
  });

  apiFunctions.setHandleRequest('assertTrue', function(test, message) {
    chromeTest.assertBool(test, true, message);
  });

  apiFunctions.setHandleRequest('assertFalse', function(test, message) {
    chromeTest.assertBool(test, false, message);
  });

  apiFunctions.setHandleRequest('assertBool',
                                function(test, expected, message) {
    if (test !== expected) {
      if (typeof(test) == "string") {
        if (message)
          message = test + "\n" + message;
        else
          message = test;
      }
      chromeTest.fail(message);
    }
  });

  apiFunctions.setHandleRequest('checkDeepEq', function(expected, actual) {
    if ((expected === null) != (actual === null))
      return false;

    if (expected === actual)
      return true;

    if (typeof(expected) !== typeof(actual))
      return false;

    for (var p in actual) {
      if ($Object.hasOwnProperty(actual, p) &&
          !$Object.hasOwnProperty(expected, p)) {
        return false;
      }
    }
    for (var p in expected) {
      if ($Object.hasOwnProperty(expected, p) &&
          !$Object.hasOwnProperty(actual, p)) {
        return false;
      }
    }

    for (var p in expected) {
      var eq = true;
      switch (typeof(expected[p])) {
        case 'object':
          eq = chromeTest.checkDeepEq(expected[p], actual[p]);
          break;
        case 'function':
          eq = (typeof(actual[p]) != 'undefined' &&
                expected[p].toString() == actual[p].toString());
          break;
        default:
          eq = (expected[p] == actual[p] &&
                typeof(expected[p]) == typeof(actual[p]));
          break;
      }
      if (!eq)
        return false;
    }
    return true;
  });

  apiFunctions.setHandleRequest('assertEq',
                                function(expected, actual, message) {
    var error_msg = "API Test Error in " + testName(currentTest);
    if (message)
      error_msg += ": " + message;
    if (typeof(expected) == 'object') {
      if (!chromeTest.checkDeepEq(expected, actual)) {
        // Note: these JSON.stringify calls may fail in tests that explicitly
        // override JSON.stringfy, so surround in try-catch.
        try {
          error_msg += "\nActual: " + JSON.stringify(actual) +
                       "\nExpected: " + JSON.stringify(expected);
        } catch (e) {}
        chromeTest.fail(error_msg);
      }
      return;
    }
    if (expected != actual) {
      chromeTest.fail(error_msg +
                       "\nActual: " + actual + "\nExpected: " + expected);
    }
    if (typeof(expected) != typeof(actual)) {
      chromeTest.fail(error_msg +
                       " (type mismatch)\nActual Type: " + typeof(actual) +
                       "\nExpected Type:" + typeof(expected));
    }
  });

  apiFunctions.setHandleRequest('assertNoLastError', function() {
    if (chrome.runtime.lastError != undefined) {
      chromeTest.fail("lastError.message == " +
                       chrome.runtime.lastError.message);
    }
  });

  apiFunctions.setHandleRequest('assertLastError', function(expectedError) {
    chromeTest.assertEq(typeof(expectedError), 'string');
    chromeTest.assertTrue(chrome.runtime.lastError != undefined,
        "No lastError, but expected " + expectedError);
    chromeTest.assertEq(expectedError, chrome.runtime.lastError.message);
  });

  apiFunctions.setHandleRequest('assertThrows',
                                function(fn, self, args, message) {
    chromeTest.assertTrue(typeof fn == 'function');
    try {
      fn.apply(self, args);
      chromeTest.fail('Did not throw error: ' + fn);
    } catch (e) {
      if (message !== undefined)
        chromeTest.assertEq(message, e.message);
    }
  });

  function safeFunctionApply(func, args) {
    try {
      if (func)
        $Function.apply(func, null, args);
    } catch (e) {
      var msg = "uncaught exception " + e;
      chromeTest.fail(msg);
    }
  };

  // Wrapper for generating test functions, that takes care of calling
  // assertNoLastError() and (optionally) succeed() for you.
  apiFunctions.setHandleRequest('callback', function(func, expectedError) {
    if (func) {
      chromeTest.assertEq(typeof(func), 'function');
    }
    var callbackCompleted = chromeTest.callbackAdded();

    return function() {
      if (expectedError == null) {
        chromeTest.assertNoLastError();
      } else {
        chromeTest.assertLastError(expectedError);
      }

      if (func) {
        safeFunctionApply(func, arguments);
      }

      callbackCompleted();
    };
  });

  apiFunctions.setHandleRequest('listenOnce', function(event, func) {
    var callbackCompleted = chromeTest.callbackAdded();
    var listener = function() {
      event.removeListener(listener);
      safeFunctionApply(func, arguments);
      callbackCompleted();
    };
    event.addListener(listener);
  });

  apiFunctions.setHandleRequest('listenForever', function(event, func) {
    var callbackCompleted = chromeTest.callbackAdded();

    var listener = function() {
      safeFunctionApply(func, arguments);
    };

    var done = function() {
      event.removeListener(listener);
      callbackCompleted();
    };

    event.addListener(listener);
    return done;
  });

  apiFunctions.setHandleRequest('callbackPass', function(func) {
    return chromeTest.callback(func);
  });

  apiFunctions.setHandleRequest('callbackFail', function(expectedError, func) {
    return chromeTest.callback(func, expectedError);
  });

  apiFunctions.setHandleRequest('runTests', function(tests) {
    chromeTest.tests = tests;
    testCount = chromeTest.tests.length;
    chromeTest.runNextTest();
  });

  apiFunctions.setHandleRequest('getApiDefinitions', function() {
    return GetExtensionAPIDefinitionsForTest();
  });

  apiFunctions.setHandleRequest('getApiFeatures', function() {
    return GetAPIFeatures();
  });
});

exports.binding = binding.generate();
