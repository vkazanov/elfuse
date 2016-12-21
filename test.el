(require 'elfuse)
(elfuse--start "mount/")

(defun elfuse--readdir-callback (path)
  ["." ".." "hello" "other" "etc"])

(let ((timer (run-at-time
              nil
              0.1
              (lambda () (let ((check-p (elfuse--check-callbacks)))
                      (unless check-p
                        (cancel-timer timer))))))) )

(elfuse--stop)
