(require 'elfuse)

(defun elfuse--readdir-callback (path)
  ["." ".." "hello" "other" "etc"])

(defun elfuse--getattr-callback (path)
  (cond
   ((equal path "/hello") 'file)
   ((equal path "/other") 'file)
   ((equal path "/etc") 'file)
   ((equal path "/") 'dir)
   (t nil)))

(elfuse--start "mount/")

(let ((timer (run-at-time
              nil
              0.1
              (lambda () (let ((check-p (elfuse--check-callbacks)))
                      (unless check-p
                        (cancel-timer timer))))))) )

(elfuse--stop)
