(require 'elfuse)
(elfuse--start "mount/")
(run-at-time
 nil
 0.1
 'elfuse--check-callbacks)
(elfuse--stop)
