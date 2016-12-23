(require 'elfuse)

(defun elfuse--readdir-callback (path)
  ["." ".." "hello" "other" "etc"])

(defun elfuse--getattr-callback (path)
  (cond
   ((equal path "/hello") [file 5])
   ((equal path "/other") [file 3])
   ((equal path "/etc") [file 2])
   ((equal path "/") [dir 0])
   (t [nil, 0])))

(elfuse--start "mount/")

(let ((timer (run-at-time
              nil
              0.1
              (lambda () (let ((check-p (elfuse--check-callbacks)))
                      (unless check-p
                        (cancel-timer timer))))))) )

(elfuse--stop)
