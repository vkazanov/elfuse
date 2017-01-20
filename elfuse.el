(require 'elfuse-module)
(require 'seq)

(defvar elfuse-time-between-checks 0.01
  "Time interval in seconds between Elfuse request checks.")

(defvar elfuse--check-timer nil
  "Timer calling the callback-responding function.")

(defconst elfuse-supported-ops-alist '((create . 1)
                                       (rename . 2)
                                       (readdir . 1)
                                       (getattr . 1)
                                       (open . 1)
                                       (release . 1)
                                       (read . 3)
                                       (write . 3)
                                       (truncate . 2))
  "An alist of Fuse op name/arity pairs supported by Elfuse.")

(defun elfuse-start (mountpath)
  "Start Elfuse using a given MOUNTPATH."
  (interactive "DElfuse mount path: ")
  (if (elfuse--dir-mountable-p mountpath)
      (let ((abspath (file-truename mountpath)))
	(elfuse--start-loop)
	(elfuse--mount abspath)
        (add-hook 'kill-emacs-hook 'elfuse--stop))
    (message "Elfuse: %s does not exist or is not empty." mountpath)))

(defun elfuse-stop ()
  "Stop Elfuse."
  (interactive)
  (elfuse--stop)
  (remove-hook 'kill-emacs-hook 'elfuse--stop))

(defmacro elfuse-define-op (opname arglist &rest body)
  "Define a Fuse operation OPNAME handler.
Apart from defining the function required by Elfuse the macro
also checks that the OPNAME is a supported Fuse operation and
there's a correct number of arguments in the ARGLIST. A list of
correct ops is defined in the `elfuse-supported-ops-alist'
variable.

Argument ARGLIST is a list of operation arguments.

Optional argument BODY is a body of the function that will handle
the operation."
  (declare (indent 2))
  (cond ((not (assq opname elfuse--supported-ops-alist))
         `(error "Operation '%s' not supported" ,(symbol-name opname)))
        ((not (= (alist-get opname elfuse--supported-ops-alist) (length arglist)))
         `(error "Operation '%s' requires %d arguments"
                 ,(symbol-name opname)
                 ,(alist-get opname elfuse--supported-ops-alist)))
        (t `(defun ,(intern (concat "elfuse--" (symbol-name opname) "-callback"))
                ,arglist
              ,@body))))

(defun elfuse--start-loop ()
  (setq elfuse--check-timer
        (run-at-time nil elfuse-time-between-checks 'elfuse--on-timer)))

(defun elfuse--on-timer ()
  (unless (elfuse--check-callbacks)
    (cancel-timer timer)
    (setq elfuse--check-timer nil)))

(defun elfuse--dir-mountable-p (path)
  (and (file-exists-p path)
       ;; only . and ..
       (= (length (directory-files path)) 2)))

(provide 'elfuse)
