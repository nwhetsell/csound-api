const Reporter = require('jasmine-terminal-reporter');

jasmine.getEnv().clearReporters();
jasmine.getEnv().addReporter(new Reporter({isVerbose: true}));
